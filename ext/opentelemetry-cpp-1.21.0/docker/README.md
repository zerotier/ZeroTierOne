# OpenTelemetry library builder

**How to use this build script:**

The build script `build.sh` can be used to build OpenTelemetry and
it's dependencies within the desired base image.
The final image only contains the necessary files in `/`.

```sh
bash build.sh -b alpine -j ${nproc} -g v1.43.2 -o v1.3.0 -t 0.14.1

# copy to current dir
docker create -ti --name otel otel-cpp-<base_image> bash
docker cp otel:/ ./
docker rm -f otel

docker create -ti --name grpc grpc-<base_image> bash
docker cp grpc:/ ./
docker rm -f grpc
```
