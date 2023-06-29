registry = us-central1-docker.pkg.dev/zerotier-central/zerotier

all:	controller-builder controller-runbase

buildx:
	@echo "docker buildx create"
	# docker run --rm --privileged multiarch/qemu-user-static --reset -p yes
	docker run --privileged --rm tonistiigi/binfmt --install all
	@echo docker buildx create --name multiarch --driver docker-container --use
	@echo docker buildx inspect --bootstrap

controller-builder:	buildx
	docker buildx build --no-cache --platform linux/amd64,linux/arm64 -t $(registry)/ctlbuild:latest -f Dockerfile.builder . --push

controller-runbase: buildx
	docker buildx build --no-cache --platform linux/amd64,linux/arm64 -t $(registry)/ctlrun:latest -f Dockerfile.run_base . --push
