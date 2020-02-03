# curl (REST API)
# User
JENKINS_USER=grant

# Api key from "/me/configure" on my Jenkins instance
JENKINS_USER_KEY=11edf2d49321321119712c46c6349eaad7

# Url for my local Jenkins instance.
JENKINS_URL=http://$JENKINS_USER:$JENKINS_USER_KEY@jenkins.int.zerotier.com

# JENKINS_CRUMB is needed if your Jenkins master has CRSF protection enabled (which it should)
JENKINS_CRUMB=`curl "$JENKINS_URL/crumbIssuer/api/xml?xpath=concat(//crumbRequestField,\":\",//crumb)"`
curl -X POST -H $JENKINS_CRUMB -F "jenkinsfile=<Jenkinsfile" $JENKINS_URL/pipeline-model-converter/validate
