#!/bin/bash

# Merely BUILDS test images

# Remove previous test results
rm _results/*.txt 

# How long we shall wait for each test to conclude
export netcon_test_wait_time=60s

export image_build_script=_build_single_image.sh

# Iterate over all depth=2 (relatively-speaking) directories and perform each test
find . -mindepth 2 -maxdepth 2 -type d | while read testdir; do
   
    if [[ $testdir != *$1* ]]
    then
	continue    
    fi

    echo "\n\n\n*** Building: '$testdir'..."
    rm _results/*.tmp

    # Stage scripts
    cp $image_build_script $testdir/$image_build_script
    cd $testdir

    # Build test docker images
    ./$image_build_script
    rm $image_build_script

    cd ../../
done
