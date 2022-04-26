#!/bin/bash

build_packages()
{
  sudo docker run -v $(pwd):/zto ztwd
}

build_container()
{
  sudo docker build -t ztwd . --load
}

"$@"
