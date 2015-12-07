# Builds a test docker image

test_name=${PWD##*/}
echo 'Building dockerfiles for test: ' "$test_name"
touch "$test_name".name

# Docker won't allow the inclusion of files outside of the build directory
cp ../../*.conf .
cp ../../zerotier-one zerotier-one
cp ../../zerotier-cli zerotier-cli
cp ../../zerotier-cli zerotier-netcon-service
cp ../../zerotier-intercept zerotier-intercept
cp ../../libzerotierintercept.so.1.0 libzerotierintercept.so.1.0
cp ../../liblwip.so liblwip.so
cp ../../netcon_identity.public netcon_identity.public
cp ../../netcon_identity.secret netcon_identity.secret
cp ../../monitor_identity.public monitor_identity.public
cp ../../monitor_identity.secret monitor_identity.secret

docker build --tag="$test_name" -f netcon_dockerfile .
docker build --tag="$test_name"_monitor -f monitor_dockerfile .

rm -f zerotier-one
rm -f zerotier-cli
rm -f zerotier-netcon-service
rm -f zerotier-intercept
rm -f libzerotierintercept.so.1.0
rm -f liblwip.so
rm -f netcon_identity.public
rm -f netcon_identity.secret
rm -f monitor_identity.public
rm -f monitor_identity.secret
rm -f *.conf
rm -f *.name