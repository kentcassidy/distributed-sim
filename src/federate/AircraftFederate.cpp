#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#include <RTI/RTI1516.h>
#include <RTI/RTIambassadorFactory.h>
#include "AircraftFederate.hpp"

using namespace std;

// Fixed identity. The controller CREATES the federation (sole creator); this federate only
// joins, so it no longer needs the FOM path.
static const wstring FEDERATION     = L"DffFederation";
static const wstring AIRCRAFT_CLASS = L"ObjectRoot.Aircraft";   // .fed root is ObjectRoot

// Wall-clock stamp for each NDJSON frame: microseconds since the Unix epoch (system_clock,
// so it is comparable ACROSS federates on the same host). METADATA ONLY -- nondeterministic,
// never part of the truth/invariance check. It exists to show that federates computed the
// same LOGICAL step at different REAL times / interleavings, while the (id, t)-keyed truth
// stays bit-identical. Integer microseconds (not a double) so log setprecision can't crush it.
static long long wallMicros() {
    using namespace std::chrono;
    return duration_cast<microseconds>(system_clock::now().time_since_epoch()).count();
}

// unique_ptr<RTIambassador> needs the complete type where it's destroyed, so the
// destructor lives here (in the .cpp) rather than being implicit in the header.
AircraftFederate::AircraftFederate() {}
AircraftFederate::~AircraftFederate() {}

////////////////////
// Public lifecycle
//////////
void AircraftFederate::run(wstring federateName) {
    this->federateName_ = federateName;      // needed by cacheHandles (fedamb.myName)

    connectToRti();
    joinFederation(federateName);
    cacheHandles();
    publishAndSubscribe();
    sendEnroll(federateName);
    waitForStart();       // block (pumping) until the controller broadcasts StartRun
    buildWorld();         // adopt exactly what we were assigned + learn the full partition map
    runLoop();            // serve loop: advance + log + hand off owned aircraft, until Shutdown
    resignAndDestroy();
}

////////////////////
// 1. Connect
//////////
void AircraftFederate::connectToRti() {
    RTIambassadorFactory factory;
    this->rtiamb.reset( factory.createRTIambassador().release() );
    this->rtiamb->connect(this->fedamb, HLA_EVOKED);   // callbacks fire only inside evoke
    wcout << L"Connected to RTI" << endl;
}

////////////////////
// 2. Join (JOIN ONLY -- the controller is the sole creator)
//////////
void AircraftFederate::joinFederation(wstring federateName) {
    // The controller creates the federation (SOLE creator -- removes the concurrent-create
    // race that split federates into isolated executions). So START THE CONTROLLER FIRST;
    // this federate only JOINS, retrying until it exists. FederationExecutionDoesNotExist =
    // controller hasn't created it yet; RTIinternalError = Portico's transient mid-create.
    const int  maxAttempts = 60;
    const auto backoff      = std::chrono::milliseconds(500);
    for (int attempt = 1; ; ++attempt) {
        try {
            this->rtiamb->joinFederationExecution(federateName, L"Aircraft", FEDERATION);
            wcout << L"Joined as " << federateName << endl;
            return;
        } catch (FederationExecutionDoesNotExist&) {
            if (attempt == 1)
                wcout << L"Waiting for the controller to create the federation..." << endl;
            if (attempt >= maxAttempts) throw;
        } catch (RTIinternalError&) {
            if (attempt >= maxAttempts) throw;
        }
        std::this_thread::sleep_for(backoff);
    }
}

////////////////////
// 3. Resolve + cache handles (only valid once joined)
//////////
void AircraftFederate::cacheHandles() {
    this->aircraftClass = rtiamb->getObjectClassHandle(AIRCRAFT_CLASS);

    this->entityIdHandle    = rtiamb->getAttributeHandle(aircraftClass, L"EntityId");
    this->massHandle        = rtiamb->getAttributeHandle(aircraftClass, L"Mass");
    this->radiusHandle      = rtiamb->getAttributeHandle(aircraftClass, L"Radius");
    this->positionHandle    = rtiamb->getAttributeHandle(aircraftClass, L"Position");
    this->velocityHandle    = rtiamb->getAttributeHandle(aircraftClass, L"Velocity");
    this->orientationHandle = rtiamb->getAttributeHandle(aircraftClass, L"Orientation");
    this->fedamb.positionHandle = this->positionHandle;

    // Enroll (federate -> controller)
    this->enrollClass        = rtiamb->getInteractionClassHandle(L"InteractionRoot.Enroll");
    this->enrollFederateName = rtiamb->getParameterHandle(enrollClass, L"FederateName");

    // AssignEntity (controller -> federate)
    this->assignClass  = rtiamb->getInteractionClassHandle(L"InteractionRoot.AssignEntity");
    this->assignTarget = rtiamb->getParameterHandle(assignClass, L"TargetFederate");
    this->assignId     = rtiamb->getParameterHandle(assignClass, L"EntityId");
    this->assignPos    = rtiamb->getParameterHandle(assignClass, L"Position");
    this->assignVel    = rtiamb->getParameterHandle(assignClass, L"Velocity");
    this->assignOrient = rtiamb->getParameterHandle(assignClass, L"Orientation");
    this->assignAngV   = rtiamb->getParameterHandle(assignClass, L"AngularV");

    // StartRun (controller -> all)
    this->startClass    = rtiamb->getInteractionClassHandle(L"InteractionRoot.StartRun");
    this->startDt       = rtiamb->getParameterHandle(startClass, L"Dt");
    this->startWorldMin = rtiamb->getParameterHandle(startClass, L"WorldMin");
    this->startWorldMax = rtiamb->getParameterHandle(startClass, L"WorldMax");
    this->startNumSteps = rtiamb->getParameterHandle(startClass, L"NumSteps");

    this->shutdownClass = rtiamb->getInteractionClassHandle(L"InteractionRoot.Shutdown");

    this->assignSectorClass = rtiamb->getInteractionClassHandle(L"InteractionRoot.AssignSector");
    this->sectorTarget = rtiamb->getParameterHandle(assignSectorClass, L"TargetFederate");
    this->sectorId     = rtiamb->getParameterHandle(assignSectorClass, L"SectorId");
    this->sectorMin    = rtiamb->getParameterHandle(assignSectorClass, L"Min");
    this->sectorMax    = rtiamb->getParameterHandle(assignSectorClass, L"Max");

    // Handoff (federate -> federate): the peer ownership transfer. We both PUBLISH it (to
    // hand an aircraft away) and SUBSCRIBE it (to adopt one).
    this->handoffClass  = rtiamb->getInteractionClassHandle(L"InteractionRoot.Handoff");
    this->handoffTarget = rtiamb->getParameterHandle(handoffClass, L"TargetFederate");
    this->handoffId     = rtiamb->getParameterHandle(handoffClass, L"EntityId");
    this->handoffPos    = rtiamb->getParameterHandle(handoffClass, L"Position");
    this->handoffVel    = rtiamb->getParameterHandle(handoffClass, L"Velocity");
    this->handoffOrient = rtiamb->getParameterHandle(handoffClass, L"Orientation");
    this->handoffAngV   = rtiamb->getParameterHandle(handoffClass, L"AngularV");
    this->handoffStep   = rtiamb->getParameterHandle(handoffClass, L"Step");

    // Give the ambassador what it needs to filter + decode the control interactions.
    fedamb.myName        = federateName_;
    fedamb.assignClass   = assignClass;
    fedamb.startClass    = startClass;
    fedamb.assignTarget  = assignTarget;
    fedamb.assignId      = assignId;
    fedamb.assignPos     = assignPos;
    fedamb.assignVel     = assignVel;
    fedamb.assignOrient  = assignOrient;
    fedamb.assignAngV    = assignAngV;
    fedamb.startDt       = startDt;
    fedamb.startWorldMin = startWorldMin;
    fedamb.startWorldMax = startWorldMax;
    fedamb.startNumSteps = startNumSteps;
    fedamb.shutdownClass = shutdownClass;
    fedamb.assignSectorClass = assignSectorClass;
    fedamb.sectorTarget  = sectorTarget;
    fedamb.sectorId      = sectorId;
    fedamb.sectorMin     = sectorMin;
    fedamb.sectorMax     = sectorMax;
    fedamb.handoffClass  = handoffClass;
    fedamb.handoffTarget = handoffTarget;
    fedamb.handoffId     = handoffId;
    fedamb.handoffPos    = handoffPos;
    fedamb.handoffVel    = handoffVel;
    fedamb.handoffOrient = handoffOrient;
    fedamb.handoffAngV   = handoffAngV;
    fedamb.handoffStep   = handoffStep;

    wcout << L"[handles] aircraft.isValid=" << aircraftClass.isValid()
          << L" assign.isValid=" << assignClass.isValid()
          << L" start.isValid="  << startClass.isValid() << endl;
}

////////////////////
// 4. Declare interest
//////////
void AircraftFederate::publishAndSubscribe() {
    AttributeHandleSet attributes;
    attributes.insert(this->positionHandle);
    rtiamb->publishObjectClassAttributes(this->aircraftClass, attributes);
    // No SUBSCRIBE to Aircraft: the constructive core is INDEPENDENT aircraft -- no ghosts,
    // no reflection. Truth is consolidated via NDJSON; ghosts belong to the Live experiment.

    rtiamb->publishInteractionClass(this->enrollClass);      // announce ourselves
    rtiamb->subscribeInteractionClass(this->assignClass);    // receive our assignments
    rtiamb->subscribeInteractionClass(this->startClass);     // receive the go signal
    rtiamb->subscribeInteractionClass(this->shutdownClass);     // receive the stop signal
    rtiamb->subscribeInteractionClass(this->assignSectorClass); // receive our sector bounds
    rtiamb->publishInteractionClass(this->handoffClass);     // hand an aircraft off to a peer
    rtiamb->subscribeInteractionClass(this->handoffClass);   // adopt an aircraft from a peer
    wcout << L"Published Aircraft.Position + Enroll + Handoff; subscribed AssignEntity + AssignSector + StartRun + Shutdown + Handoff" << endl;
}

////////////////////
// 5. Announce ourselves to the controller (Enroll interaction)
//////////
void AircraftFederate::sendEnroll(wstring federateName) {
    // Let declaration management SETTLE first: a freshly-joined federate may not yet know
    // the controller SUBSCRIBES to Enroll, and an interaction sent before that subscription
    // is known is routed to nobody. Pump a few callbacks so the pub/sub picture propagates,
    // THEN send.
    for (int i = 0; i < 5; ++i) this->rtiamb->evokeMultipleCallbacks(0.05, 0.1);

    ParameterHandleValueMap params;
    params[this->enrollFederateName] = encodeString(federateName);
    VariableLengthData tag((void*)"enroll", 7);
    this->rtiamb->sendInteraction(this->enrollClass, params, tag);
    wcout << L"Enrolled with the controller as " << federateName << endl;

    for (int i = 0; i < 3; ++i) this->rtiamb->evokeMultipleCallbacks(0.02, 0.05);
}

////////////////////
// 6. Wait for the controller: pump callbacks until StartRun latches
//////////
void AircraftFederate::waitForStart() {
    wcout << L"Waiting for assignments + StartRun from the controller..." << endl;
    // AssignEntity messages arrive BEFORE StartRun (the controller sends them first), so
    // once startReceived latches, our assignments are already collected. Drain a little
    // extra afterward to catch any straggler.
    const int maxSpins = 6000;   // generous upper bound (~ up to 10 min at 0.1s)
    int spins = 0;
    while (!fedamb.startReceived && spins < maxSpins) {
        rtiamb->evokeMultipleCallbacks(0.1, 0.2);
        ++spins;
    }
    for (int i = 0; i < 5; ++i) rtiamb->evokeMultipleCallbacks(0.05, 0.1);

    if (!fedamb.startReceived)
        throw std::runtime_error("timed out waiting for StartRun from the controller");

    this->dt_       = fedamb.dt;
    this->numSteps_ = fedamb.numSteps;   // run length is the controller's to set, not ours
}

////////////////////
// 7. Build our world from EXACTLY what the controller assigned
//////////
void AircraftFederate::buildWorld() {
    // Sort assignments by id so owned_ order is deterministic regardless of the order the
    // AssignEntity interactions happened to arrive -- partition invariance requires the
    // per-aircraft truth to be independent of who/what order computes it.
    std::vector<EntitySpec> specs = fedamb.assignments;
    std::sort(specs.begin(), specs.end(),
              [](const EntitySpec& a, const EntitySpec& b){ return a.id < b.id; });

    for (size_t i = 0; i < specs.size(); ++i) {
        const EntitySpec& spec = specs[i];
        AircraftParams params;          // shared filler defaults (fidelity is out of scope)
        params.id = spec.id;
        world_.owned().push_back(Aircraft(spec.id, params, &model_));
        world_.owned().back().state() = spec.initial;
        acStep_[spec.id] = 0;   // scenario aircraft start at logical step 0 (the initial state)

        // one HLA object instance per owned aircraft
        ObjectInstanceHandle h = rtiamb->registerObjectInstance(aircraftClass);
        ownedObjects[spec.id] = h;
    }

    // Install our sector(s) from the controller's AssignSector, so we can detect an aircraft
    // drifting out of our region (a handoff candidate). Also keep the WHOLE partition map, so
    // we can compute -- ourselves, peer-to-peer -- which federate a departing aircraft goes to.
    for (size_t i = 0; i < fedamb.assignedSectors.size(); ++i)
        world_.addSector(fedamb.assignedSectors[i]);
    partitionMap_ = fedamb.partitionMap;
    wcout << L"Installed " << fedamb.assignedSectors.size() << L" own sector(s); partition map has "
          << partitionMap_.size() << L" region(s)" << endl;

    // NDJSON log: one file per federate. meta line first. (sectors:[] for now -- 5c will
    // populate it from the disseminated partition so the viewer can draw the boxes.)
    string fname(federateName_.begin(), federateName_.end());
    log_.open("sim_out/" + fname + ".ndjson");
    // 17 significant digits = max_digits10 for double, so the truth round-trips to the exact
    // same bits -- required for the diff to verify BIT-EXACT invariance (not just ~9 digits).
    log_ << std::setprecision(17);
    log_ << "{\"meta\":{\"federate\":\"" << fname << "\",\"dt\":" << dt_
         << ",\"sectors\":[]}}\n";

    wcout << L"World ready: " << world_.owned().size()
          << L" owned aircraft; logging to " << federateName_ << L".ndjson" << endl;
}

////////////////////
// 8. Serve loop: advance each owned aircraft ONE dt per iteration, log it, hand it off if it
//    left our sector -- and keep serving (ready to ADOPT peer handoffs) until the controller
//    broadcasts Shutdown. There is NO global step counter and NO catch-up: each aircraft
//    carries its own step (acStep_), so a handed-off aircraft simply continues its sequence on
//    its new owner. Each aircraft is advanced exactly once per logical step -- by whoever owns
//    it at that step -- and logged per-aircraft at its own step, so the assembled truth is
//    bit-identical to a never-handed-off run regardless of how the federates' loops interleave.
//////////
void AircraftFederate::runLoop() {
    const int STEPS = numSteps_;   // from the controller's StartRun, not a local constant

    // Step 0 = the INITIAL state, logged by whoever first owns the aircraft (scenario aircraft
    // here). An ADOPTED aircraft gets no step-0 line -- its earlier steps were logged by its
    // previous owner, up to and including the transfer step.
    for (const Aircraft& ac : world_.owned()) emitRecord(ac, 0);

    wcout << L"[" << federateName_ << L"] serving until Shutdown (target "
          << STEPS << L" steps per aircraft)" << endl;

    while (!fedamb.shutdownReceived) {
        drainHandoffs();   // adopt any aircraft peers have handed to us since last iteration

        std::vector<Aircraft>& owned = world_.owned();   // re-taken each pass (drain may grow it)
        size_t k = 0;
        while (k < owned.size()) {
            EntityId id = owned[k].id();
            if (acStep_[id] >= STEPS) { ++k; continue; }   // finished; hold (still owned)

            owned[k].advance(dt_);               // integrate this ONE aircraft by exactly one dt
            acStep_[id] += 1;
            emitRecord(owned[k], acStep_[id]);   // log it at its own new logical step

            if (acStep_[id] == STEPS && !finishedPrinted_.count(id)) {
                finishedPrinted_.insert(id);
                wcout << L"[" << federateName_ << L"] aircraft " << id
                      << L" completed all " << STEPS << L" steps" << endl;
            }

            if (tryDepart(k)) continue;   // left my sector -> handed off / lost + released (owned[] shifted)
            ++k;                          // still mine -> advance to the next aircraft
        }

        // Announce the busy<->idle transition, so the operator can tell "still crunching" from
        // "just holding" -- matters once a federate owns many aircraft. "Idle" = nothing left
        // to compute right now (every owned aircraft is at STEPS, or I own none); it RE-ARMS if
        // work returns (a late adoption), so a second "done" line genuinely means done again.
        size_t pending = 0;
        for (size_t j = 0; j < owned.size(); ++j) if (acStep_[owned[j].id()] < STEPS) ++pending;
        if (pending == 0 && !idleAnnounced_) {
            idleAnnounced_ = true;
            wcout << L"[" << federateName_ << L"] all current tasks done -- " << owned.size()
                  << L" owned aircraft at step " << STEPS << L"; idle, serving until Shutdown" << endl;
        } else if (pending > 0) {
            idleAnnounced_ = false;   // work (re)appeared -> re-arm the announcement
        }

        rtiamb->evokeMultipleCallbacks(0.05, 0.1);   // flush/receive handoffs; keep controller serviced
    }
    wcout << L"[" << federateName_ << L"] Shutdown received; resigning." << endl;
}

// One NDJSON line for ONE aircraft at its OWN logical step. Also puts the aircraft's real
// Position on the wire (vestigial in the constructive core -- nobody subscribes -- but keeps
// the HLA object model honest for the later Live experiment). `owner` = this federate, so
// ownership is EXPLICIT in the stream: the viewer colors the plane by its current owner and
// can show it change hands, rather than inferring from which file the row appears in.
void AircraftFederate::emitRecord(const Aircraft& ac, long long step) {
    const State& s = ac.state();

    AttributeHandleValueMap attrs;
    attrs[positionHandle] = encodeVec3(s.position);
    VariableLengthData tag((void*)"pos", 4);
    rtiamb->updateAttributeValues(ownedObjects[ac.id()], attrs, tag);

    std::string owner(federateName_.begin(), federateName_.end());
    log_ << "{\"t\":" << (step * dt_) << ",\"wt\":" << wallMicros()
         << ",\"aircraft\":[{\"id\":" << ac.id()
         << ",\"owner\":\"" << owner << "\",\"role\":\"owned\""
         << ",\"pos\":["  << s.position.x << "," << s.position.y << "," << s.position.z << "]"
         << ",\"vel\":["  << s.velocity.x << "," << s.velocity.y << "," << s.velocity.z << "]"
         << ",\"quat\":[" << s.attitude.x << "," << s.attitude.y << "," << s.attitude.z << "," << s.attitude.w
         << "]}]}\n";
}

// Adopt any handoffs peers have sent us since we last drained. We take the EXACT transferred
// state at the transfer step and do NOT log that step (the previous owner already did) -- so
// coverage stays exactly-once. The next serve-loop iteration advances it to step+1 and logs on.
void AircraftFederate::drainHandoffs() {
    std::vector<HandoffIn>& q = fedamb.incomingHandoffs;   // callbacks only ever APPEND to this
    for (; handoffsAdopted_ < q.size(); ++handoffsAdopted_) {
        const HandoffIn& h = q[handoffsAdopted_];

        AircraftParams params;
        params.id = h.spec.id;
        world_.owned().push_back(Aircraft(h.spec.id, params, &model_));
        world_.owned().back().state() = h.spec.initial;   // exact transferred state
        acStep_[h.spec.id] = h.step;                       // continue this aircraft's step sequence
        ownedObjects[h.spec.id] = rtiamb->registerObjectInstance(aircraftClass);
        finishedPrinted_.erase(h.spec.id);                 // it's ours now; re-arm the "done" print

        wcout << L"[" << federateName_ << L"] adopted aircraft " << h.spec.id
              << L" at step " << h.step << L" (handoff)" << endl;
    }
}

// If owned[k] has left every sector I own, hand it to the peer that owns the region it entered
// (computed from the broadcast map, half-open), or declare it lost if it left the whole world.
// Returns true if the aircraft was released (so the caller must NOT advance its index).
bool AircraftFederate::tryDepart(size_t k) {
    const Aircraft& ac = world_.owned()[k];
    const Vec3& p = ac.state().position;
    if (inMySector(p)) return false;   // still inside my region -> keep it

    EntityId id = ac.id();
    long long step = acStep_[id];
    std::wstring dest = ownerOf(p);

    if (dest.empty()) {
        // Left the whole world: nobody owns where it went. Log it and stop simulating it.
        wcout << L"[" << federateName_ << L"] aircraft " << id
              << L" LEFT THE WORLD at step " << step << L" (pos " << p
              << L") -- lost in the void" << endl;
    } else if (dest == federateName_) {
        // Detection (inclusive) vs destination (half-open) can disagree exactly on a seam; if
        // the map still says this region is mine, keep the aircraft (harmless, converges next step).
        return false;
    } else {
        sendHandoff(dest, ac, step);
        wcout << L"[" << federateName_ << L"] aircraft " << id
              << L" left my sector at step " << step << L" -> handoff to " << dest << endl;
    }

    releaseAircraft(id, k);
    return true;
}

// Directed peer transfer: send this aircraft's exact state + the step it's valid at, straight
// to the destination federate. The controller is NOT involved.
void AircraftFederate::sendHandoff(const wstring& dest, const Aircraft& ac, long long step) {
    const State& s = ac.state();
    ParameterHandleValueMap p;
    p[handoffTarget] = encodeString(dest);
    p[handoffId]     = encodeUint32(ac.id());
    p[handoffPos]    = encodeVec3(s.position);
    p[handoffVel]    = encodeVec3(s.velocity);
    p[handoffOrient] = encodeQuat(s.attitude);
    p[handoffAngV]   = encodeVec3(s.angularV);
    p[handoffStep]   = encodeUint32(static_cast<uint32_t>(step));
    VariableLengthData tag((void*)"handoff", 8);
    rtiamb->sendInteraction(handoffClass, p, tag);
}

// Drop an aircraft we no longer own: delete its HLA instance and forget it.
void AircraftFederate::releaseAircraft(EntityId id, size_t k) {
    std::map<EntityId, ObjectInstanceHandle>::iterator it = ownedObjects.find(id);
    if (it != ownedObjects.end()) {
        VariableLengthData tag((void*)"handoff", 8);
        try { rtiamb->deleteObjectInstance(it->second, tag); }
        catch (const rti1516e::Exception&) { /* best effort: nobody subscribes in the core */ }
        ownedObjects.erase(it);
    }
    acStep_.erase(id);
    std::vector<Aircraft>& owned = world_.owned();
    owned.erase(owned.begin() + k);
}

// Inside any sector I own? Inclusive (Sector::contains). An empty sector set = no partitioning
// configured (e.g. K=1 whole-world) -> everything is "mine" and nothing is ever handed off.
bool AircraftFederate::inMySector(const Vec3& p) const {
    const std::vector<Sector>& secs = world_.sectors();
    if (secs.empty()) return true;
    for (size_t i = 0; i < secs.size(); ++i) if (secs[i].contains(p)) return true;
    return false;
}

// Which federate owns point p? Scan the full partition map with the UNIFORM HALF-OPEN rule
// ([min,max) on every axis) -- the same rule the controller's slabOf() used to assign
// ownership, so a departing aircraft goes to exactly the federate the controller would pick.
// Returns L"" if p is outside every region (it left the world -> lost in the void).
wstring AircraftFederate::ownerOf(const Vec3& p) const {
    for (size_t i = 0; i < partitionMap_.size(); ++i) {
        const Sector& s = partitionMap_[i].sector;
        if (p.x >= s.min.x && p.x < s.max.x &&
            p.y >= s.min.y && p.y < s.max.y &&
            p.z >= s.min.z && p.z < s.max.z)
            return partitionMap_[i].owner;
    }
    return wstring();
}

////////////////////
// 9. Tear down. The CONTROLLER (creator) destroys the federation; we only resign.
//////////
void AircraftFederate::resignAndDestroy() {
    VariableLengthData tag((void*)"bye", 4);
    for (map<EntityId, ObjectInstanceHandle>::iterator it = ownedObjects.begin();
         it != ownedObjects.end(); ++it) {
        rtiamb->deleteObjectInstance(it->second, tag);
    }

    try {
        rtiamb->resignFederationExecution(NO_ACTION);
        wcout << L"Resigned from federation" << endl;
    } catch (const rti1516e::Exception& e) {
        // Should not happen now that the controller pumps callbacks during the run, but if
        // resign coordination ever times out, still disconnect cleanly rather than crash.
        wcerr << L"Resign failed (" << e.what() << L"); disconnecting anyway" << endl;
    }

    rtiamb->disconnect();
    wcout << L"Disconnected from RTI" << endl;
}
