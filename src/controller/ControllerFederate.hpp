#pragma once

#include <memory>
#include <string>
#include <RTI/RTI1516.h>
#include "ControllerFedAmb.hpp"

using namespace rti1516e;
using namespace std;

// ControllerFederate -- the federation manager (ADR-0015). It is the SOLE creator of the
// federation (which removes the concurrent-create race that split federates into isolated
// executions) and the coordinator of config-driven partitioning. Lifecycle:
//
//   connect -> create (sole) -> join -> subscribe Enroll / publish AssignEntity+StartRun
//   -> collect the roster while federates join -> on the operator's ENTER:
//        K = roster size; load scenario; tile the world into K slabs; assign entities;
//        send one AssignEntity per entity; broadcast StartRun (dt + world bounds)
//   -> stay joined to hold the federation alive for the run -> tear down on a 2nd ENTER.
//
// Authority note: the controller ASSIGNS who owns what; it is not an arbiter of physics.
// Single ownership is what makes each aircraft's truth authoritative (see ADR-0019).
class ControllerFederate {
public:
    ControllerFederate();
    ~ControllerFederate();

    void run(const wstring& scenarioPath);

private:
    void connectToRti();
    void createAndJoin();                 // controller is the sole CREATOR
    void cacheHandles();
    void publishAndSubscribe();
    void collectRosterUntilStart();       // wait for the operator; drain enrollments
    void partitionAndDisseminate(const wstring& scenarioPath);
    void awaitShutdown();                 // hold the federation alive during the run
    void resignAndDestroy();

    unique_ptr<RTIambassador> rtiamb;
    ControllerFedAmb          fedamb;

    // Interaction + parameter handles (resolved once after join).
    InteractionClassHandle enrollClass;
    InteractionClassHandle assignClass;
    InteractionClassHandle startClass;
    InteractionClassHandle shutdownClass;      // broadcast to end the run
    InteractionClassHandle assignSectorClass;  // per-federate sector bounds

    ParameterHandle enrollFederateName;
    ParameterHandle assignTargetFederate;
    ParameterHandle assignEntityId;
    ParameterHandle assignPosition;
    ParameterHandle assignVelocity;
    ParameterHandle assignOrientation;
    ParameterHandle assignAngularV;
    ParameterHandle startDt;
    ParameterHandle startWorldMin;
    ParameterHandle startWorldMax;
    ParameterHandle startNumSteps;
    ParameterHandle sectorTarget, sectorId, sectorMin, sectorMax;
};
