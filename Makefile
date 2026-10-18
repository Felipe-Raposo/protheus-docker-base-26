export DOCKER_BUILDKIT := 1

IMAGE_REPO := feliperaposo
NAME := $(IMAGE_REPO)/protheus-docker-base
VERSION := 26
MINOR_VERSION := 26.3.0.0

.PHONY: build release release_latest

build:
	docker image build --platform=linux/amd64 -t $(NAME):$(VERSION) -t $(NAME):$(MINOR_VERSION) -t $(NAME):latest .

release: build
	docker image push $(NAME):$(VERSION)
	docker image push $(NAME):$(MINOR_VERSION)

release_latest: release
	docker image push $(NAME):latest
