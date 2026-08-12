# EduPerf CloudLab backend

This directory turns one dedicated CloudLab `m510` into a short-lived,
authenticated measurement service for the EduPerf VS Code extension. It runs
only the 100 shipped PerfBank adapters; it cannot execute uploaded source or an
arbitrary shell command.

## Deployment

1. In CloudLab, create a **repository-based profile** using
   `https://github.com/proywm/eduperf-cloudlab-deploy.git`.
2. Select the repository's top-level `profile.py`.
   CloudLab uses the top-level `profile.py` and clones the repository to
   `/local/repository` on the node.
3. Instantiate the profile. The default is the paper's `m510` hardware type.
4. Wait for **Ready**, then copy `/local/eduperf/connection.json` from the node.
5. In VS Code, run **EduPerf: Connect to CloudLab Backend** and select that file.

The first standard-image launch builds pinned HPCToolkit 2024.01.1 through a
pinned Spack checkout and can take substantial time. Capture that configured
node as a project image, update the profile's `disk_image` parameter to the
captured image URN, and use that immutable image for the class. Routine launches
then only rotate the per-experiment credential, refresh the fixed adapters, and
start the service.

## Measurement isolation

- CloudLab allocates one bare-metal `m510`.
- systemd pins the worker to CPU 0.
- the API queue executes exactly one job at a time.
- each comparison uses the same seven alternating before/after rounds as the
  paper adapters.
- the API accepts a fixed case ID and action; no source upload or command field
  exists.
- TLS and a random 256-bit bearer token protect the control-network API.

All 100 cases support fresh runtime execution. The five source-attributed cases
currently marked `hpctoolkit` also support fresh hardware-counter and calling-
context collection. Other cases return an explicit unsupported-profile error;
the backend never presents recorded evidence as a live hardware profile.

## Operations

The health check is `GET /v1/health`. Authenticated endpoints are
`GET /v1/cases`, `POST /v1/runs`, and `GET /v1/runs/:id`. The service and setup
logs live in the system journal and `/local/eduperf/bootstrap.log`; these are for
the instructor/administrator and are never exposed in the student extension.
