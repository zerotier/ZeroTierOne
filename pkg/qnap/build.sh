#!/bin/bash

build_packages()
{
  sudo docker run -v $(pwd):/zto ztqnap
}

build_container()
{
  sudo docker build -t ztqnap . --load
}

"$@"
