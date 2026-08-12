"""EduPerf CloudLab measurement service.

This profile allocates one dedicated m510 bare-metal node and provisions the
EduPerf fixed-workload worker.  It preserves the hardware family used for the
PerfBank measurements and serializes student requests on CPU 0.

Instructions:

Wait until the experiment is **Ready**. The backend is then available at
`https://{host-worker}:8443`.

The instructor downloads `/local/eduperf/connection.json` once and distributes
that small connection file to the class. Students choose **EduPerf: Connect to
CloudLab Backend** in VS Code; they never use a terminal or profiler command.

For the first image-building launch, keep `install_hpctoolkit` enabled. That
launch builds the pinned profiling stack and may take substantial time. Capture
the configured node as a project disk image, then set `disk_image` to its URN.
Subsequent classroom experiments start from that immutable image and the
bootstrap script only refreshes credentials and starts the service.
"""

import geni.portal as portal
import geni.rspec.pg as rspec


portal.context.defineParameter(
    "hardware_type",
    "CloudLab physical node type",
    portal.ParameterType.NODETYPE,
    "m510",
    legalValues=[("m510", "Utah m510 (paper protocol)")],
    advanced=True,
)
portal.context.defineParameter(
    "disk_image",
    "Boot image URN",
    portal.ParameterType.IMAGE,
    "urn:publicid:IDN+emulab.net+image+emulab-ops//UBUNTU22-64-STD",
    advanced=True,
    longDescription=(
        "Use the standard image for the initial build. After capturing the "
        "configured node, replace this value with the project image URN."
    ),
)
portal.context.defineParameter(
    "install_hpctoolkit",
    "Build HPCToolkit when it is not already installed",
    portal.ParameterType.BOOLEAN,
    True,
    advanced=True,
)

params = portal.context.bindParameters()
request = portal.context.makeRequestRSpec()

worker = request.RawPC("worker")
worker.hardware_type = params.hardware_type
worker.disk_image = params.disk_image
worker.addService(
    rspec.Execute(
        shell="bash",
        command="sudo -n /local/repository/cloudlab-backend/bootstrap.sh",
    )
)

portal.context.printRequestRSpec()
