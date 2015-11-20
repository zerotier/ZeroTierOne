To run unit tests:

1) Place a blank network config file in this directory (e.g. "e5cd7a9e1c5311ab.conf")
 - This will be used to inform test-specific scripts what network to use for testing (create your own, or use the given at your own risk)

2) run build.sh
 - Builds ZeroTier with Network Containers enabled
 - Builds LWIP library
 - Builds library harness
 - Copies all aformentioned files into unit test directory to be used for building docker files

3) run test.sh
 - Will (for now, one at a time), execute each unit test's (test.sh)



A) Each unit test's test.sh will:
 - temporarily copy all built files into local directory
 - build test container
 - build monitor container
 - remove temporary files
 - run each container and perform test

B) Results will be written to the 'zerotierone/docker-test/_result' directory
 - Results will be a combination of raw and formatted dumps to files whose names reflect the test performed
 - In the event of failure, 'FAIL.' will be appended to the result file's name
  - (e.g. FAIL.my_application_1.0.2.x86_64)
 - In the event of success, 'OK.' will be appended
