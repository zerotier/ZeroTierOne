local registry = "084037375216.dkr.ecr.us-east-2.amazonaws.com";

local targets = [
      { "os": "linux",   "name": "sid", "isas": [ "386", "armv7", "amd64", "arm64" ], "events": [ "push", "tag", "custom" ] },
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
      "image": registry + "/honda-builder",
      "commands": [
        "aws ecr get-login-password --region us-east-2 | docker login --username AWS --password-stdin " + registry,
        "./ci/scripts/build.sh " + platform + " " + isa + " " + "100.0.0+${DRONE_COMMIT_SHA:0:8}" + " " + "${DRONE_BUILD_EVENT}"
      ]
    },
    // {
    //   "name": "list",
    //   "image": registry + "/honda-builder",
    //   "commands": [ "ls -la " + platform ]
    // },
  ],  
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
