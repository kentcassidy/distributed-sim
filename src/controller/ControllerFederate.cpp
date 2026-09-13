#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <vector>
#include <sys/select.h>    // non-blocking stdin (Linux/container) so we can pump while waiting
#include <unistd.h>
#include <RTI/RTI1516.h>
#include <RTI/RTIambassadorFactory.h>
#include "ControllerFederate.hpp"
#include "Encoding.hpp"     // ../federate -- shared RTI wire encoding
#include "Scenario.hpp"     // dff_core
#include "Partition.hpp"    // dff_core

using namespace std;

// Shared federation identity. Duplicated from AircraftFederate.cpp for now; a small shared
// FederationConfig header is a cheap future cleanup (noted).
static const wstring FEDERATION = L"DffFederation";
static const wstring FOM_MODULE = L"foms/dff-fom.fed";

// The run config the controller OWNS and disseminates. World bounds are half-open
// [min,max) (the Partition rule), chosen with margin so the two hand-authored aircraft sit
// cleanly inside and the K=2 y-seam (y=750 for a [0,1500) span) falls between their lanes
// (500 and 1000). worldMax.x is generous but only matters at t=0 -- ownership is static.
static const Vec3   WORLD_MIN  = Vec3(0.0,     0.0,    -500.0);
static const Vec3   WORLD_MAX  = Vec3(20000.0, 1500.0,  500.0);
static const Axis   SPLIT_AXIS = Axis::Y;
static const double DT         = 0.1;

// Non-blocking check for ENTER on stdin (Linux/container). Returns true once the operator
// hits ENTER, consuming the line. This lets the controller keep PUMPING callbacks while it
// waits -- essential, because as the federation's creator/coordinator it must service the
// channel (so other federates can resign cleanly) rather than block unresponsive on getline.
static bool enterPressed() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv;
    tv.tv_sec  = 0;
    tv.tv_usec = 0;
    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        std::string line;
        std::getline(std::cin, line);
        return true;
    }
    return false;
}

ControllerFederate::ControllerFederate() {}
ControllerFederate::~ControllerFederate() {}

void ControllerFederate::run(const wstring& scenarioPath) {
    connectToRti();
    createAndJoin();
    cacheHandles();
    publishAndSubscribe();
    collectRosterUntilStart();
    partitionAndDisseminate(scenarioPath);
    awaitShutdown();
    resignAndDestroy();
}

////////////////////
// 1. Connect
//////////
void ControllerFederate::connectToRti() {
    RTIambassadorFactory factory;
    this->rtiamb.reset(factory.createRTIambassador().release());
    this->rtiamb->connect(this->fedamb, HLA_EVOKED);
    wcout << L"[controller] connected to RTI" << endl;
}

////////////////////
// 2-3. Create (SOLE creator) + join
//////////
void ControllerFederate::createAndJoin() {
    // The controller is meant to be the ONLY process that creates the federation -- start
    // it first. AlreadyExists here means a stale execution from a previous run (or a
    // federate raced ahead); we still join, but it's worth noting.
    try {
        vector<wstring> fom;
        fom.push_back(FOM_MODULE);
        rtiamb->createFederationExecution(FEDERATION, fom);
        wcout << L"[controller] created federation " << FEDERATION
              << L" (from " << FOM_MODULE << L")" << endl;
    } catch (FederationExecutionAlreadyExists&) {
        wcout << L"[controller] federation already existed; joining it "
                 L"(expected the controller to be first)" << endl;
    }

    rtiamb->joinFederationExecution(L"controller", L"Controller", FEDERATION);
    wcout << L"[controller] joined as 'controller'" << endl;
}

////////////////////
// 4. Resolve + cache interaction/parameter handles (valid only once joined)
//////////
void ControllerFederate::cacheHandles() {
    enrollClass = rtiamb->getInteractionClassHandle(L"InteractionRoot.Enroll");
    assignClass = rtiamb->getInteractionClassHandle(L"InteractionRoot.AssignEntity");
    startClass  = rtiamb->getInteractionClassHandle(L"InteractionRoot.StartRun");

    enrollFederateName   = rtiamb->getParameterHandle(enrollClass, L"FederateName");
    assignTargetFederate = rtiamb->getParameterHandle(assignClass, L"TargetFederate");
    assignEntityId       = rtiamb->getParameterHandle(assignClass, L"EntityId");
    assignPosition       = rtiamb->getParameterHandle(assignClass, L"Position");
    assignVelocity       = rtiamb->getParameterHandle(assignClass, L"Velocity");
    assignOrientation    = rtiamb->getParameterHandle(assignClass, L"Orientation");
    startDt              = rtiamb->getParameterHandle(startClass,  L"Dt");
    startWorldMin        = rtiamb->getParameterHandle(startClass,  L"WorldMin");
    startWorldMax        = rtiamb->getParameterHandle(startClass,  L"WorldMax");

    // Hand the enroll handles to the ambassador so its callback can match + decode.
    fedamb.enrollClass       = enrollClass;
    fedamb.federateNameParam = enrollFederateName;

    wcout << L"[controller] handles cached (enroll.isValid=" << enrollClass.isValid()
          << L", assign.isValid=" << assignClass.isValid()
          << L", start.isValid="  << startClass.isValid() << L")" << endl;
}

////////////////////
// 5. Declare interest: hear enrollments, speak assignments + start
//////////
void ControllerFederate::publishAndSubscribe() {
    rtiamb->subscribeInteractionClass(enrollClass);   // federates announce themselves
    rtiamb->publishInteractionClass(assignClass);     // we send per-entity assignments
    rtiamb->publishInteractionClass(startClass);      // we send the go signal
    wcout << L"[controller] subscribed Enroll; publishing AssignEntity + StartRun" << endl;
}

////////////////////
// 6. Collect the roster, then wait for the operator to start the run
//////////
void ControllerFederate::collectRosterUntilStart() {
    wcout << L"\n[controller] Start the aircraft federates now." << endl;
    wcout << L"[controller] Press ENTER once they have all joined to begin the run." << endl;

    // Pump callbacks LIVE while waiting: enrolls arrive and print as they come, and the
    // channel stays serviced. Break when the operator hits ENTER.
    while (!enterPressed()) {
        rtiamb->evokeMultipleCallbacks(0.1, 0.2);
    }

    if (fedamb.roster.empty()) {
        wcout << L"[controller] WARNING: no federates enrolled -- nothing to partition." << endl;
    }
}

////////////////////
// 7. Partition the world by K = roster size, then disseminate
//////////
void ControllerFederate::partitionAndDisseminate(const wstring& scenarioPath) {
    // Freeze the roster. Sort by name so the federate -> slab mapping is deterministic run
    // to run. (The consolidated truth is owner-independent, so this is for debuggability,
    // not correctness.)
    vector<wstring> roster = fedamb.roster;
    sort(roster.begin(), roster.end());
    unsigned int K = static_cast<unsigned int>(roster.size());
    if (K == 0) return;

    // Load the shared scenario (loadScenario takes a narrow path).
    string path(scenarioPath.begin(), scenarioPath.end());
    Scenario scn = loadScenario(path);

    // Tile the volume into K slabs and assign each entity by its initial position.
    vector<Sector>    sectors    = tileVolume(WORLD_MIN, WORLD_MAX, SPLIT_AXIS, K);
    map<EntityId,int> assignment = assignEntities(scn, WORLD_MIN, WORLD_MAX, SPLIT_AXIS, K);

    // Show the slab geometry so the spatial partition is legible. Split axis is Y here, so
    // print each slab's half-open Y range and its owner -- including any IDLE slab whose
    // range holds no aircraft (that owner simply gets no AssignEntity).
    wcout << L"\n[controller] K=" << K << L": world Y[" << WORLD_MIN.y << L"," << WORLD_MAX.y
          << L") split into " << K << L" slab(s):" << endl;
    for (size_t i = 0; i < sectors.size(); ++i) {
        wcout << L"  slab " << i << L": y in [" << sectors[i].min.y << L", "
              << sectors[i].max.y << L") -> owner " << roster[i] << endl;
    }

    // Write the controller's partition descriptor for the VIEWER: one meta-only line with the
    // world bounds + every sector and its owner. The viz reads THIS authoritative file to draw
    // the world/sector boxes, instead of inferring geometry from per-federate truth. It has no
    // aircraft frames, so dff_diff ignores it (contributes no (id,step) points).
    {
        std::ofstream desc("sim_out/controller.ndjson");
        desc << std::setprecision(17);
        desc << "{\"meta\":{\"federate\":\"controller\",\"dt\":" << DT
             << ",\"world\":{\"min\":[" << WORLD_MIN.x << "," << WORLD_MIN.y << "," << WORLD_MIN.z << "]"
             << ",\"max\":[" << WORLD_MAX.x << "," << WORLD_MAX.y << "," << WORLD_MAX.z << "]}"
             << ",\"sectors\":[";
        for (size_t i = 0; i < sectors.size(); ++i) {
            const Sector& s = sectors[i];
            std::string owner(roster[i].begin(), roster[i].end());   // ASCII names
            if (i) desc << ",";
            desc << "{\"id\":" << s.id
                 << ",\"owner\":\"" << owner << "\""
                 << ",\"min\":[" << s.min.x << "," << s.min.y << "," << s.min.z << "]"
                 << ",\"max\":[" << s.max.x << "," << s.max.y << "," << s.max.z << "]}";
        }
        desc << "]}}\n";
    }
    wcout << L"[controller] wrote sim_out/controller.ndjson (world + " << sectors.size()
          << L" sectors)" << endl;

    wcout << L"[controller] assigning " << assignment.size() << L" entit(y/ies):" << endl;

    // One AssignEntity per entity -> the federate owning its slab.
    VariableLengthData assignTag((void*)"assign", 7);
    for (map<EntityId,int>::const_iterator it = assignment.begin(); it != assignment.end(); ++it) {
        EntityId          id    = it->first;
        int               slab  = it->second;
        const wstring&    owner = roster[slab];
        const EntitySpec* spec  = scn.find(id);
        if (!spec) continue;   // cannot happen: assignment ids come from scn

        ParameterHandleValueMap p;
        p[assignTargetFederate] = encodeString(owner);
        p[assignEntityId]       = encodeUint32(id);
        p[assignPosition]       = encodeVec3(spec->initial.position);
        p[assignVelocity]       = encodeVec3(spec->initial.velocity);
        p[assignOrientation]    = encodeQuat(spec->initial.attitude);
        rtiamb->sendInteraction(assignClass, p, assignTag);

        wcout << L"  entity " << id << L" -> slab " << slab << L" -> " << owner << endl;
    }

    // Broadcast the go signal + the shared run config.
    ParameterHandleValueMap s;
    s[startDt]       = encodeDouble(DT);
    s[startWorldMin] = encodeVec3(WORLD_MIN);
    s[startWorldMax] = encodeVec3(WORLD_MAX);
    VariableLengthData startTag((void*)"start", 6);
    rtiamb->sendInteraction(startClass, s, startTag);
    wcout << L"[controller] StartRun broadcast (dt=" << DT << L")." << endl;

    // Nudge the callback pump so the outgoing interactions flush before we idle.
    for (int i = 0; i < 5; ++i) rtiamb->evokeMultipleCallbacks(0.02, 0.05);
}

////////////////////
// 8. Hold the federation alive for the run
//////////
void ControllerFederate::awaitShutdown() {
    // The controller is the CREATOR; destroying now would yank the federation out from
    // under the still-running aircraft federates. So stay JOINED and KEEP PUMPING: as the
    // coordinator we must service the channel so the aircraft federates can resign cleanly
    // (a blocked, non-pumping controller makes their resignFederationExecution time out).
    wcout << L"\n[controller] Run in progress. Press ENTER to tear down the federation."
          << endl;
    while (!enterPressed()) {
        rtiamb->evokeMultipleCallbacks(0.1, 0.2);
    }
}

////////////////////
// 9. Tear down
//////////
void ControllerFederate::resignAndDestroy() {
    rtiamb->resignFederationExecution(NO_ACTION);
    wcout << L"[controller] resigned" << endl;

    try {
        rtiamb->destroyFederationExecution(FEDERATION);
        wcout << L"[controller] destroyed federation" << endl;
    } catch (FederatesCurrentlyJoined&) {
        wcout << L"[controller] others still joined; leaving federation for them" << endl;
    } catch (FederationExecutionDoesNotExist&) {
        wcout << L"[controller] federation already gone" << endl;
    }

    rtiamb->disconnect();
    wcout << L"[controller] disconnected" << endl;
}
