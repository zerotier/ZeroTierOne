Kubernetes + ZeroTier
====

A self-authorizing Kubernetes cluster deployment over a private ZeroTier network.

This is a quick tutorial for setting up a Kubernetes deployment which can self-authorize each new replica onto your private ZeroTier network with no additional configuration needed when you scale. The Kubernetes-specific instructions and content is based on the [hellonode](http://kubernetes.io/docs/hellonode/) tutorial. All of the files discussed below can be found [here]();



## Preliminary tasks

**Step 1: Go to [my.zerotier.com](https://my.zerotier.com) and generate an API key. This key will be used by ZeroTier to automatically authorize new instances of your VMs to join your deployment network during replication.**

**Step 2: Create a new `private` network. Take note of the network ID, henceforth: `nwid`**

**Step 3: Follow the instructions from the [hellonode](ttp://kubernetes.io/docs/hellonode/) tutorial and set up your development system (install Google Cloud SDK).**




## Construct docker container

**Step 4: Create necessary files for inclusion in Dockerfile**
 - `mkdir ztkube`

Add the following files to the `ztkube` directory. These files will be compiled into the Docker image.
 
 - Create an empty `<nwid>.conf` file to specify the private deployment network you created in *Step 2*:

 - The CLI tool config file `.zerotierCliSettings` should contain your API keys to authorize new devices on your network. In this example the default controller is hosted by us at [my.zerotier.com](https://my.zerotier.com). Alternatively, you can host your own network controller but you'll need to modify the CLI config file accordingly.

```
{
  "configVersion": 1,
  "defaultCentral": "@my.zerotier.com",
  "defaultController": "@my.zerotier.com",
  "defaultOne": "@local",
  "things": {
    "local": {
      "auth": "XXXXXXXXXXXXXXXXXXXXXXXX",
      "type": "one",
      "url": "http://127.0.0.1:9993/"
    },
    "my.zerotier.com": {
      "auth": "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX",
      "type": "central",
      "url": "https://my.zerotier.com/"
    },
  }
}
```


 - The `Dockerfile` will copy the ZeroTier service as well as the ZeroTier CLI to the image: 

```
FROM node:4.4
EXPOSE 8080
COPY server.js .
COPY zerotier .
COPY zerotier-cli .
COPY entrypoint.sh .
COPY .zerotierCliSettings <settings path>?>
CMD node server.js
```

 - The `entrypoint.sh` script will start the ZeroTier service in the VM, attempt to join your deployment network and automatically authorize the new VM if your network is set to private:

```
./zerotier 
zerotier-cli join $(NWID).conf
zerotier-cli net-auth $(NWID) $(DEVID)
```

**Step 5: Lastly, build the image:**

`docker build -t gcr.io/$PROJECT_ID/hello-node .`



**Step 6: Push the docker image to your *Container Registry***

`gcloud docker push gcr.io/$PROJECT_ID/hello-node:v1`


## Deploy!

**Step 7: Create Kubernetes Cluster**

`gcloud config set compute/zone us-central1-a`

`gcloud container clusters create hello-world`

`gcloud container clusters get-credentials hello-world`



**Step 8: Create your pod**

`kubectl run hello-node --image=gcr.io/$PROJECT_ID/hello-node:v1 --port=8080`



**Step 9: Scale**

`kubectl scale deployment hello-node --replicas=4`

## Verify

Now, after a minute or so you can use `zerotier-cli net-members <nwid>` to show all of your VM instances on your ZeroTier deployment network. If you haven't [configured your local CLI](https://github.com/zerotier/ZeroTierOne/tree/dev/cli), you can simply log into [my.zerotier.com](https://my.zerotier.com), go to *Networks -> nwid* to check that your VMs are indeed members of your private network.
