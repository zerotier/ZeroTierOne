//
// tweakables
//

local registry = "084037375216.dkr.ecr.us-east-2.amazonaws.com";
local build_channel = "zerotier-builds";
local release_channel = "zerotier-releases";

local targets = [
    // { "os": "linux", distro: "redhat", "name": "el9",      "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "tag", "custom" ] },
    // { "os": "linux", distro: "redhat", "name": "el8",      "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "tag" ] },
    // { "os": "linux", distro: "redhat", "name": "el7",      "isas": [ "386",          "amd64",          "ppc64le"],                                  "events": [ "tag" ] },
    // { "os": "linux", distro: "amazon", "name": "amzn2",    "isas": [                 "amd64", "arm64" ],                                            "events": [ "tag" ] },
    // { "os": "linux", distro: "amazon", "name": "amzn2022", "isas": [                 "amd64", "arm64" ],                                            "events": [ "tag" ] },
    // { "os": "linux", distro: "fedora", "name": "fc38",     "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "tag" ] },
    // { "os": "linux", distro: "fedora", "name": "fc37",     "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "tag" ] },
    // { "os": "linux", distro: "fedora", "name": "fc36",     "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "tag" ] },
    // { "os": "linux", distro: "ubuntu", "name": "jammy",    "isas": [        "armv7", "amd64", "arm64", "ppc64le", "s390x", "riscv64" ],             "events": [ "tag" ] },
    // { "os": "linux", distro: "ubuntu", "name": "focal",    "isas": [        "armv7", "amd64", "arm64", "ppc64le", "s390x", "riscv64" ],             "events": [ "tag" ] },
    // { "os": "linux", distro: "ubuntu", "name": "bionic",   "isas": [ "386", "armv7", "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "tag" ] },
    // { "os": "linux", distro: "ubuntu", "name": "xenial",   "isas": [ "386", "armv7", "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "tag" ] },
    // { "os": "linux", distro: "ubuntu", "name": "trusty",   "isas": [ "386", "armv7", "amd64", "arm64" ],                                            "events": [ "tag" ] },
    // { "os": "linux", distro: "debian", "name": "bookworm", "isas": [ "386", "armv7", "amd64", "arm64", "mips64le", "ppc64le", "s390x" ],            "events": [ "tag"] },
    // { "os": "linux", distro: "debian", "name": "bullseye", "isas": [ "386", "armv7", "amd64", "arm64", "mips64le", "ppc64le", "s390x" ],            "events": [ "push", "tag", "custom" ] },
    // { "os": "linux", distro: "debian", "name": "buster",   "isas": [ "386", "armv7", "amd64", "arm64" ],                                            "events": [ "tag" ] },
    // { "os": "linux", distro: "debian", "name": "stretch",  "isas": [ "386", "armv7", "amd64", "arm64" ],                                            "events": [ "tag" ] },
    // { "os": "linux", distro: "debian", "name": "jessie",   "isas": [ "386", "armv7", "amd64" ],                                                     "events": [ "tag" ] },

    { "os": "linux", distro: "redhat", "name": "el9",      "isas": [ "amd64" ],  "events": [ "push", "tag", "custom" ] },
//    { "os": "linux", distro: "ubuntu", "name": "jammy",    "isas": [ "amd64" ],  "events": [ "push", "tag", "custom" ] },
    { "os": "linux", distro: "ubuntu", "name": "jessie",    "isas": [ "amd64" ],  "events": [ "push", "tag", "custom" ] },
// { "os": "windows", distro: "windows", "name": "windows",  "isas": [ "amd64" ], "events": [ "push", "tag", "custom" ] },
 // { "os": "darwin", distro: "darwin", "name": "darwin",  "isas": [ "amd64" ], "events": [ "push", "tag", "custom" ] },

];

local less_targets = [
      { "os": "linux", distro: "redhat", "name": "el9",      "isas": [                 "amd64", "arm64" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "redhat", "name": "el8",      "isas": [                 "amd64", "arm64" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "ubuntu", "name": "jammy",    "isas": [        "armv7", "amd64", "arm64" ],             "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "ubuntu", "name": "focal",    "isas": [        "armv7", "amd64", "arm64" ],             "events": [ "push", "tag", "custom" ] },
];

local master_targets = [
      //
      // copypasta from here
      //
      { "os": "linux", distro: "redhat", "name": "el9",      "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "redhat", "name": "el8",      "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "redhat", "name": "el7",      "isas": [ "386", "amd64",          "ppc64le"],                                  "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "amazon", "name": "amzn2",    "isas": [                 "amd64", "arm64" ],                                            "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "amazon", "name": "amzn2022", "isas": [                 "amd64", "arm64" ],                                            "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "fedora", "name": "fc38",     "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "fedora", "name": "fc37",     "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "fedora", "name": "fc36",     "isas": [                 "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "ubuntu", "name": "jammy",    "isas": [        "armv7", "amd64", "arm64", "ppc64le", "s390x", "riscv64" ],             "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "ubuntu", "name": "focal",    "isas": [        "armv7", "amd64", "arm64", "ppc64le", "s390x", "riscv64" ],             "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "ubuntu", "name": "bionic",   "isas": [ "386", "armv7", "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "ubuntu", "name": "xenial",   "isas": [ "386", "armv7", "amd64", "arm64", "ppc64le", "s390x" ],                        "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "ubuntu", "name": "trusty",   "isas": [ "386", "armv7", "amd64", "arm64" ],                                            "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "debian", "name": "sid",      "isas": [ "386", "armv7", "amd64", "arm64", "mips64le", "ppc64le", "s390x", "riscv64" ], "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "debian", "name": "bookworm", "isas": [ "386", "armv7", "amd64", "arm64", "mips64le", "ppc64le", "s390x" ],            "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "debian", "name": "bullseye", "isas": [ "386", "armv7", "amd64", "arm64", "mips64le", "ppc64le", "s390x" ],            "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "debian", "name": "buster",   "isas": [ "386", "armv7", "amd64", "arm64" ],                                            "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "debian", "name": "stretch",  "isas": [ "386", "armv7", "amd64", "arm64" ],                                            "events": [ "push", "tag", "custom" ] },
      { "os": "linux", distro: "debian", "name": "jessie",   "isas": [ "386", "armv7", "amd64" ],                                                     "events": [ "push", "tag", "custom" ] },
      { "os": "windows", distro: "windows", "name": "win2k22", "isas": [ "amd64" ],                                                                     "events": [ "push", "tag", "custom" ] }
];

//
// functions
//

local pipeline_type(os)  = if os == "darwin" then "exec" else "docker";
local builder_image(os)  = if os == "linux" then registry + "/honda-builder" else registry + "/windows-builder";
local tester_image(os)   = if os == "linux" then registry + "/honda-builder" else registry + "/windows-tester";
local build_step_volumes(os) = if os == "linux" then [ { name: "zerotier-builds", path: "/zerotier-builds" } ] else [];
local release_step_volumes(os) = if os == "linux" then [ { name: "zerotier-releases", path: "/zerotier-releases" } ] else [];
local host_volumes(os)   = if os == "linux" then [
  { name: "zerotier-builds", host: { path: "/zerotier-builds" } },
  { name: "zerotier-releases", host: { path: "/zerotier-releases" } },
] else [];

local index_image(distro) =
      if distro == "debian" || distro == "ubuntu" then
          registry + "/apt-builder"
      else if distro == "redhat" || distro == "fedora" || distro == "amazon" then
          registry + "/dnf-builder"
      else if distro == "windows" then
          registry + "/msi-builder"
;          

local copy_commands(os, distro, name, isa, version) =
  if os == "linux" then [
      std.join(" ", [ "./ci/scripts/publish.sh", name, distro, isa, version, "${DRONE_BUILD_EVENT}" ])
    ]
    else if os == "windows" then [  
      "C:\\scripts\\fix-ec2-metadata.ps1",
      "Get-ChildItem windows",
      // "aws s3 cp windows\\bytey-SetupFiles\\bytey.msi s3://zerotier-builds/windows/" + version + "/bytey.msi",
    ] else if os == "darwin" then [
        "echo hello"
      ]
;

local index_commands(os, channel, distro, name, isas) =
      if os == "linux" then
        [ "/usr/local/bin/index " + channel + " " + distro + " " + name  + " " + std.join(" ", isas) ]
      else if os == "windows" then
        [ "Get-ChildItem -Recurse windows" ]
;

local build_commands(os, distro, name, isa, version) =
      if os == "linux" then
        [ std.join(" ", [ "./ci/scripts/build.sh", name, distro, isa, version, "${DRONE_BUILD_EVENT}" ]) ]
      else
        if os == "windows" then
           [ "windows/build.ps1", "windows/package.ps1" ]
      else
        if os == "darwin" then
           [ "whoami" ]
;

local test_commands(os, distro, name, isa, version) =
  if os == "linux" then
    [ std.join(" ", [ "./ci/scripts/test.sh", name, distro, isa, version, "${DRONE_BUILD_EVENT}" ]) ]
  else
    if os == "windows" then
      [ "windows/testpackage.ps1 " + version ]
;

//
// render
//

local Build(os, distro, name, isa, events) = {
  "kind": "pipeline",
  "type": pipeline_type(os),
  "name": std.join(" ", [ name, isa, "build" ]),
  "pull": "always",
  "clone": { "depth": 1, [ if os == "darwin" then "disable" ]: true },
  "steps": [
    {
      "name": "build",
      "image": builder_image(os),
      "commands": build_commands(os, distro, name, isa, "100.0.0+${DRONE_COMMIT_SHA:0:8}"),
      "when": { "event": [ "push" ]},
    },
    {
      "name": "release",
      "image": builder_image(os),
      "commands": build_commands(os, distro, name, isa, "${DRONE_TAG}"),
      "when": { "event": [ "tag" ]},
    },    
    {
      "name": "copy build",
      "image": builder_image(os),
      "commands": copy_commands(os, distro, name, isa, "100.0.0+${DRONE_COMMIT_SHA:0:8}"),
      "volumes": build_step_volumes(os),
      "when": { "event": [ "push" ]},
    },
    {
      "name": "copy relase",
      "image": builder_image(os),
      "commands": copy_commands(os, distro, name, isa, "${DRONE_TAG}"),
      "volumes": release_step_volumes(os),
      "when": { "event": [ "tag" ]},      
    },    
  ],
  "volumes": host_volumes(os),
  "platform": { "os": os, [ if isa == "arm64" || isa == "armv7" then "arch" ]: "arm64" },
  "trigger": { "event": events }
};

local Test(os, distro, name, isa, events) = {
  "kind": "pipeline",
  "type": pipeline_type(os),
  "name": std.join(" ", [ name, isa, "test"]),
  "pull": "always",
  "clone": { "depth": 1 },
  "steps": [
    {
      "name": "test build",
      "image": tester_image(os),
      "volumes": build_step_volumes(os),
      "commands": test_commands(os, distro, name, isa, "100.0.0+${DRONE_COMMIT_SHA:0:8}"),
      "when": { "event": [ "push" ]},
    },
    {
      "name": "test release",
      "image": tester_image(os),
      "volumes": release_step_volumes(os),
      "commands": test_commands(os, distro, name, isa, "${DRONE_TAG}"),
      "when": { "event": [ "tag" ]},
    },    
  ],
  "volumes": host_volumes(os),
  "platform": { "os": os, [ if isa == "arm64" || isa == "armv7" then "arch" ]: "arm64" },
  "depends_on": [ std.join(" ", [ name, "index" ]) ],
  "trigger": { "event": events }
};

local Index(p) = {
  "kind": "pipeline",
  "type": pipeline_type(p.os),
  "name": std.join(" ", [ p.name, "index" ]),
  "pull": "always",
  "clone": { "depth": 1 },
  "steps": [
    {
      "name": "index build",
      "image": index_image(p.distro),
      "commands": index_commands(p.os, "zerotier-builds", p.distro, p.name, p.isas),
      "volumes": build_step_volumes(p.os),
      "environment":{ "GPG_PRIVATE_KEY": { from_secret: "gpg-private-key" }},
      "when": { "event": [ "push" ]},
    },
    {
      "name": "index release",
      "image": index_image(p.distro),
      "commands": index_commands(p.os, "zerotier-releases", p.distro, p.name, p.isas),
      "volumes": release_step_volumes(p.os),
      "environment":{ "GPG_PRIVATE_KEY": { from_secret: "gpg-private-key" }},
      "when": { "event": [ "tag" ]},  
    },    
  ],
  "volumes": host_volumes(p.os),
  "platform": { "os": p.os },
  depends_on: std.flattenArrays([ [ std.join(" ", [ p.name, isa, "build" ]) ] for isa in p.isas ]),
  "trigger": { "event": p.events }
};

//
// print
//

std.flattenArrays([
    [
      Build(p.os, p.distro, p.name, isa, p.events)
        for isa in p.isas
    ] +
    [
      Index(p)
    ]
    for p in targets
 ]) +
 std.flattenArrays([
     [
        Test(p.os, p.distro, p.name, isa, p.events)
         for isa in p.isas
     ]
     for p in targets
 ])
 