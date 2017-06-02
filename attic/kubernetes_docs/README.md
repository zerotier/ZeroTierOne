Kubernetes + ZeroTier
====

A self-authorizing Kubernetes cluster deployment over a private ZeroTier network.

This is a quick tutorial for setting up a Kubernetes deployment which can self-authorize each new replica onto your private ZeroTier network with no additional configuration needed when you scale. The Kubernetes-specific instructions and content is based on the [hellonode](http://kubernetes.io/docs/hellonode/) tutorial. All of the files discussed below can be found [here]();



## Preliminary tasks

**Step 1: Go to [my.zerotier.com](https://my.zerotier.com) and generate a network controller API key. This key will be used by ZeroTier to automatically authorize new instances of your VMs to join your secure deployment network during replication.**

**Step 2: Create a new `private` network. Take note of the network ID, henceforth: `nwid`**

**Step 3: Follow the instructions from the [hellonode](ttp://kubernetes.io/docs/hellonode/) tutorial to set up your development system.**

***
## Construct docker image

**Step 4: Create necessary files for inclusion into image, your resultant directory should contain:**

 - `ztkube/<nwid>.conf`
 - `ztkube/Dockerfile`
 - `ztkube/entrypoint.sh`
 - `ztkube/server.js`
 - `ztkube/zerotier-cli`
 - `ztkube/zerotier-one`

Start by creating a build directory to copy all required files into `mkdir ztkube`. Then build the following:
 - `make one`
 - `make cli`

Add the following files to the `ztkube` directory. These files will be compiled into the Docker image.
 
 - Create an empty `<nwid>.conf` file to specify the private deployment network you created in *Step 2*:

 - Create a CLI tool config file `.zerotierCliSettings` which should only contain your network controller API key to authorize new devices on your network (the local service API key will be filled in automatically). In this example the default controller is hosted by us at [my.zerotier.com](https://my.zerotier.com). Alternatively, you can host your own network controller but you'll need to modify the CLI config file accordingly.

```
{
  "configVersion": 1,
  "defaultCentral": "@my.zerotier.com",
  "defaultController": "@my.zerotier.com",
  "defaultOne": "@local",
  "things": {
    "local": {
      "auth": "local_service_auth_token_replaced_automatically",
      "type": "one",
      "url": "http://127.0.0.1:9993/"
    },
    "my.zerotier.com": {
      "auth": "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
      "type": "central",
      "url": "https://my.zerotier.com/"
    }
  }
}
```


 - Create a `Dockerfile` which will copy the ZeroTier service as well as the ZeroTier CLI to the image: 

```
FROM node:4.4
EXPOSE 8080/tcp 9993/udp

# Install ZT network conf files
RUN mkdir -p /var/lib/zerotier-one/networks.d
ADD *.conf /var/lib/zerotier-one/networks.d/
ADD *.conf /
ADD zerotier-one /
ADD zerotier-cli /
ADD .zerotierCliSettings /

# Install App
ADD server.js /

# script which will start/auth VM on ZT network
ADD entrypoint.sh /
RUN chmod -v +x /entrypoint.sh 

CMD ["./entrypoint.sh"]
```

 - Create the `entrypoint.sh` script which will start the ZeroTier service in the VM, attempt to join your deployment network and automatically authorize the new VM if your network is set to private:

```
#!/bin/bash

echo '*** ZeroTier-Kubernetes self-auth test script'
chown -R daemon /var/lib/zerotier-one
chgrp -R daemon /var/lib/zerotier-one
su daemon -s /bin/bash -c '/zerotier-one -d -U -p9993 >>/tmp/zerotier-one.out 2>&1'
dev=""
nwconf=$(ls *.conf)
nwid="${nwconf%.*}"

sleep 10
dev=$(cat /var/lib/zerotier-one/identity.public| cut -d ':' -f 1)

echo '*** Joining'
./zerotier-cli join "$nwid".conf
# Fill out local service auth token
AUTHTOKEN=$(cat /var/lib/zerotier-one/authtoken.secret)
sed "s|\local_service_auth_token_replaced_automatically|${AUTHTOKEN}|" .zerotierCliSettings > /root/.zerotierCliSettings
echo '*** Authorizing'
./zerotier-cli net-auth @my.zerotier.com "$nwid" "$dev"
echo '*** Cleaning up' # Remove controller auth token
rm -rf .zerotierCliSettings /root/.zerotierCliSettings
node server.js
```

**Step 5: Build the image:**

 - `docker build -t gcr.io/$PROJECT_ID/hello-node .`



**Step 6: Push the docker image to your *Container Registry***

 - `gcloud docker push gcr.io/$PROJECT_ID/hello-node:v1`

***
## Deploy!

**Step 7: Create Kubernetes Cluster**

 - `gcloud config set compute/zone us-central1-a`

 - `gcloud container clusters create hello-world`

 - `gcloud container clusters get-credentials hello-world`



**Step 8: Create your pod**

 - `kubectl run hello-node --image=gcr.io/$PROJECT_ID/hello-node:v1 --port=8080`



**Step 9: Scale**

 - `kubectl scale deployment hello-node --replicas=4`

***
## Verify

Now, after a minute or so you can use `zerotier-cli net-members <nwid>` to show all of your VM instances on your ZeroTier deployment network. If you haven't [configured your local CLI](https://github.com/zerotier/ZeroTierOne/tree/dev/cli), you can simply log into [my.zerotier.com](https://my.zerotier.com), go to *Networks -> nwid* to check that your VMs are indeed members of your private network. You should also note that the `entrypoint.sh` script will automatically delete your network controller API key once it has authorized your VM. This is merely a security measure and can be removed if needed.
