#!/bin/bash

# Merely RUNS test images

# Remove previous test results
rm _results/*.txt 

# How long we shall wait for each test to conclude
export netcon_test_wait_time=60s

# Test structure, in later releases more complex multi-party scripts will be included
export test_script=two_party_test.sh

# Iterate over all depth=2 (relatively-speaking) directories and perform each test
find . -mindepth 2 -maxdepth 2 -type d | while read testdir; do
    echo "*** Testing: '$testdir'..."
    rm _results/*.tmp

    # Stage scripts
    cp $test_script $testdir/$test_script
    cd $testdir

    # Run test 
    ./$test_script
    rm $test_script

    cd ../../
done