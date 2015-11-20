
# Docker won't allow the inclusion of files outside of the build directory
cp ../../zerotier-one zerotier-one
cp ../../zerotier-cli zerotier-cli
cp ../../intercept intercept
cp ../../libintercept.so.1.0 libintercept.so.1.0
cp ../../liblwip.so liblwip.so
cp ../../netcon_identity.public netcon_identity.public
cp ../../netcon_identity.secret netcon_identity.secret
cp ../../monitor_identity.public monitor_identity.public
cp ../../monitor_identity.secret monitor_identity.secret


docker build --tag="httpd_demo" -f netcon_dockerfile .
docker build --tag="httpd_demo_monitor" -f monitor_dockerfile .

rm -f zerotier-one
rm -f zerotier-cli
rm -f intercept
rm -f libintercept.so.1.0
rm -f liblwip.so
rm -f netcon_identity.public
rm -f netcon_identity.secret
rm -f monitor_identity.public
rm -f monitor_identity.secret

# Start netcon container to be tested
docker run -d -it -v $PWD/../../_results:/opt/results --device=/dev/net/tun httpd_demo:latest
#./start_monitor.sh
docker run -d -it -v $PWD/../../_results:/opt/results --device=/dev/net/tun httpd_demo_monitor:latest

sleep 45s
docker kill $(docker ps -a -q)
#docker ps -a -q --filter="name=httpd_demo"
#docker ps -a -q --filter="name=httpd_demo_monitor"

#rm -rf ../../_tmp/*.txt