
local targets = [
      //
      // Render these into .drone.yaml by running "make drone"
      //
      { "os": "linux", "name": "el9", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "el8", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "el7", "isas": [ "amd64", "ppc64le"], "events": [ "tag" ] },
      { "os": "linux", "name": "el6", "isas": [ "amd64" ], "events": [ "tag" ] },
      { "os": "linux", "name": "amzn2", "isas": [ "amd64", "arm64" ], "events": [ "tag" ] },
      { "os": "linux", "name": "amzn2022", "isas": [ "amd64", "arm64" ], "events": [ "tag", "push" ] },
      { "os": "linux", "name": "fc37", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "fc36", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "fc35", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "jammy", "isas": [ "amd64", "arm64", "armv7", "riscv64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "focal", "isas": [ "amd64", "arm64", "armv7", "riscv64", "ppc64le"  ], "events": [ "tag" ] },
      { "os": "linux", "name": "bionic", "isas": [ "amd64", "arm64", "386", "ppc64le", "s390x" ], "events": ["tag" ] },
      { "os": "linux", "name": "xenial", "isas": [ "amd64", "arm64", "386" ], "events": [ "tag" ] },
      { "os": "linux", "name": "sid", "isas": [ "386", "amd64", "arm64",  "riscv64", "mips64le", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "bookworm", "isas": [ "amd64", "arm64", "armv7", "386", "mips64le", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "bullseye", "isas": [ "amd64", "arm64", "armv7", "386", "mips64le", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "buster", "isas": [ "amd64", "arm64", "armv7", "386", "mips64le", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "stretch", "isas": [ "amd64", "arm64", "386" ], "events": [ "tag" ] },
      // { "os": "windows", "name": "win2k19", "isas": [ "amd64" ], "events": ["push", "tag" ] }
];

local master_targets = [
      //
      // Render these into .drone.yaml by running "make drone"
      //      
      { "os": "linux", "name": "el9", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "el8", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "el7", "isas": [ "amd64", "ppc64le"], "events": [ "tag" ] },
      { "os": "linux", "name": "el6", "isas": [ "amd64" ], "events": [ "tag" ] },
      { "os": "linux", "name": "amzn2", "isas": [ "amd64", "arm64" ], "events": [ "tag" ] },
      { "os": "linux", "name": "fc37", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "fc36", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "fc35", "isas": [ "amd64", "arm64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "jammy", "isas": [ "amd64", "arm64", "armv7", "riscv64", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "focal", "isas": [ "amd64", "arm64", "armv7", "riscv64", "ppc64le"  ], "events": [ "tag" ] },
      { "os": "linux", "name": "bionic", "isas": [ "amd64", "arm64", "386", "ppc64le", "s390x" ], "events": ["tag" ] },
      { "os": "linux", "name": "xenial", "isas": [ "amd64", "arm64", "386" ], "events": [ "tag" ] },
      { "os": "linux", "name": "sid", "isas": [ "386", "amd64", "arm64",  "riscv64", "mips64le", "ppc64le", "s390x" ], "events": [ "push", "tag" ] },
      { "os": "linux", "name": "bookworm", "isas": [ "amd64", "arm64", "armv7", "386", "mips64le", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "bullseye", "isas": [ "amd64", "arm64", "armv7", "386", "mips64le", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "buster", "isas": [ "amd64", "arm64", "armv7", "386", "mips64le", "ppc64le", "s390x" ], "events": [ "tag" ] },
      { "os": "linux", "name": "stretch", "isas": [ "amd64", "arm64", "386" ], "events": [ "tag" ] },
      // { "os": "windows", "name": "win2k19", "isas": [ "amd64" ], "events": ["push", "tag" ] }
];

local Build(platform, os, isa, events) = {
  "kind": "pipeline",
  "type": "docker",
  "pull": "always",
  "name": platform + " " + isa + " " + "build",
  "clone": { "depth": 1 },
  "steps": [
    {
      "name": "build",
      "image": "registry.sean.farm/honda-builder",
      "commands": [ "./ci/scripts/build.sh " + platform + " " + isa + " " + "100.0.0+${DRONE_COMMIT_SHA:0:8}" + " " + "${DRONE_BUILD_EVENT}" ]
    },
    {
      "name": "list",
      "image": "registry.sean.farm/honda-builder",
      "commands": [ "ls -la " + platform ]
    },
    // {
    //   "name": "notify-mattermost",
    //   "image": "registry.sean.farm/mattermost-notify",
    //   "environment": {
    //     "token": { "from_secret": "mattermost-token" },
    //     "host": { "from_secret": "mattermost-host" },
    //     "channel": { "from_secret": "mattermost-channel" },
    //     "maxRetry": 3,
    //   },
    //   "when": { "status": [ "failure" ] }
    // }
  ],  
  "image_pull_secrets": [ "dockerconfigjson" ],
  [ if isa == "arm64" || isa == "armv7" then "platform" ]: { os: os, arch: "arm64" },
  "trigger": { "event": events }
};

// puttin on the bits

std.flattenArrays([
  [
     Build(p.name, p.os, isa, p.events)
      for isa in p.isas
  ]
  for p in targets
])
