#!/bin/bash

build_packages()
{
  sudo docker run -v $(pwd):/zto ztasustor
}

build_container()
{
  sudo docker build -t ztasustor . --load
}

"$@"
