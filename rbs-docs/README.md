# Replacement Build System (RBS)

RBS is a research project aimed to take the existing features of systems
such as Jenkins, JenkinsX, GitHub Actions, etc. and provide a more modern take
on a source control-agnostic system that is self-hostable and flexible in the
workloads that it supports. The primary focus is to provide a plug-and-play
set of basic building blocks that can be assembled together in a cohesive
fashion to tune a build/workflow environment to the needs of various
organizations.

## Success Criteria

1. Fully config management capable
2. Swappable components to tailor operations for differences in environments
3. Self-hostable
4. Out-of-box support for local and containerized workloads
5. Zero downtime updates/upgrades for supported components
6. Alternatives to local disk storage

## Non-Goals

1. Coupling with a particular source control system/provider
2. Strict opinionation on workload (cloud/k8s enforcement)

## Alternates

The following are other products where inspiration has been drawn from:

### Jenkins

Jenkins is the primary impetus for this project after managing deployments of
it for the last couple of years. The system is highly flexible and tested across
decades with wide deployment. At scale, the system can become difficult to
manage without extensive work in ensuring plugin compatibility, system
resource management, and potentially splitting off the system into focused
instances that require additional management strategies to workaround some
of the more difficult problems.

Groovy scripting is extremely powerful here, though generally not what
most people are familiar with. This lends itself to complex automation
features that can be developed separately but other design decisions also
result in a programming model where one has to be careful where their code
is actually running (controller versus node) so that they don't introduce
subtle bugs into their custom steps.

### Jenkins X

A more modern attempt at Jenkins with a heavy focus on containerized workflows.
This specific focus was the main drawback of this platform as it would have
required extensive workarounds in order to get existing pipelines to behave
properly.

### Bamboo

Proprietary system from Atlassian. Self-hostable like Jenkins, has a number
of parallels from a management perspective. Standard drawbacks of running
proprietary software were the main issue here as an explicit goal is the
debuggability and manageability of the system, even in the face of code errors,
a fact which we can never escape.

### GitHub Actions/Gitea Actions/GitLab CI

Generally fit the same architecture with a YAML-based configuration language
which serves as a workflow engine that is tightly couples with a code hosting
platform. The familiarity with YAML and the overall popularity of these formats
are big wins for onboarding existing users and their tight integration provides
a very polished experience. We'd rather be agnostic in this regard but should make
the most common options as smooth as possible.

### Zuul

One of the few options with an explicit Gerrit integration, a YAML basis,
self-hosting, rolling upgrades, and a unique code gating feature. Of the alternates,
this is likely two be the second largest source of inspiration outside of Jenkins.
The one drawback here is the explicit focus on CI/CD where we would like to try and
merge generic workflow management.

## Architecture (WIP)

```
*--------------*    *------------------*
| Web Frontend | -> | Config Subsystem |
*--------------*    *------------------*
     |             /           |
     |            /            |
*---------------*   *------------------*
| Observability | - | Trigger Handlers |
*---------------*   *------------------*
     |            \            |
     |             \           |
*---------*    *-----------*   |
| Workers | <- | Job Queue |<-/
*---------*    *-----------*
```

## Phase 1 - Basic System Skeleton

The initial round is centered around develping composable pieces that
we can plug-and-play in order to have a rapid level of experimentation
and initial flexibility. The general goals are to have:

1. A working copy of each subsystem in a simplistic form
2. Demonstration of basic reliability features on a single system
3. Locally runnable suite with single-command startup/teardown
4. Binary protocol reference implementations, documents

## Phase 2 - Extensibility

As a second round, we want to examine what it may be like to allow users
of the system to define their own job tasks, something commonplace in
many of the solutions available on the market. Furthermore this is a good
place to look into system management options that we can provide in a
version-controlled manner. Deliverables:

1. User-defined tasks
2. Configuration-as-code management of applicable system settings

## Phase 3 - Inter-machine Distribution

A final pass before polishing, the next evolution would be to expand the
solutions out to working across machines. This is mostly a matter of
taking the protocols created in phase 1 and ensuring that we have new means
of communication across the network, rather than OS primitives. Deliverables:

1. Equivalents of each system, or their interfaces, for networked use
2. Recovery/reliability features for new network failure modes

## Phase 4 - Polishing/Hardening

At the end, assuming we've made it this far, there should be an extensive
review and cleanup of code to ensure it meets performance goals, security
requirements, and maintainability bars that one would expect out of a
production-ready piece of software. The above also includes the production
of quality documentation for all aspects of the system that a user may need
to interact with.
