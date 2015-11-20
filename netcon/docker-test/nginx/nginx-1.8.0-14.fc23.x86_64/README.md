Network Containers Test Sequence for: httpd_demo

1) Creates a Netcon docker container with an intercepted instance of httpd
2) Creates a test monitor container 

3) Test monitor container waits for X seconds for the Netcon container to come online
4) Test monitor attempts to curl an index.html file
 - If file is of sufficient size, it will append "OK." to the result file for this test, otherwise "FAIL." is appended
