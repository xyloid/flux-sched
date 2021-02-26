FROM fluxrm/flux-core:bionic

ARG USER=flux
ARG UID=1000

# Install extra buildrequires for flux-sched:
RUN sudo apt-get update
RUN sudo apt-get -qq install -y --no-install-recommends \
	libboost-graph-dev \
	libboost-system-dev \
	libboost-filesystem-dev \
	libboost-regex-dev \
	libxml2-dev \
	python-yaml \
	libyaml-cpp-dev \
		golang-1.13-go

# Add configured user to image with sudo access:
#
RUN \
 if test "$USER" != "flux"; then  \
      sudo groupadd -g $UID $USER \
   && sudo useradd -g $USER -u $UID -d /home/$USER -m $USER \
   && sudo sh -c "printf \"$USER ALL= NOPASSWD: ALL\\n\" >> /etc/sudoers" \
   && sudo adduser $USER sudo ; \
 fi
ENV PATH="$PATH:/usr/src/golang/go/bin"
ENV GOPATH=/usr/src/resource/hlapi/bindings/go/
USER $USER
WORKDIR /home/$USER
