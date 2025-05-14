############################################################
#  Base image + OCCT prerequisites
############################################################
FROM node:18-slim

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      ca-certificates \
      libgl1-mesa-glx libxrender1 libsm6 libxext6 \
      build-essential cmake git curl \
      tcl-dev tk-dev libfreetype6-dev \
      libx11-dev libxext-dev libxt-dev libxmu-dev libxi-dev \
      libgl1-mesa-dev \
      nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

############################################################
#  Build & install OCCT 7.9
############################################################
WORKDIR /tmp
RUN git clone --branch V7_9_0 \
        https://git.dev.opencascade.org/repos/occt.git occt && \
    mkdir -p occt/build && cd occt/build && \
    cmake .. \
      -DCMAKE_INSTALL_PREFIX=/usr/local \
      -DBUILD_MODULE_Draw=OFF \
      -DBUILD_MODULE_Visualization=OFF && \
    make -j"$(nproc)" install && \
    cd / && rm -rf /tmp/occt

############################################################
#  Configure dynamic linker for OCCT libs
############################################################
RUN echo "/usr/local/lib" > /etc/ld.so.conf.d/occt.conf && \
    ldconfig

############################################################
#  Copy only the things the Node app needs
############################################################
WORKDIR /app

# 1) your API's manifest(s)
COPY server/package*.json ./

# 2) your Prisma schema
COPY prisma ./prisma

# install & generate Prisma client, then prune to prod deps
RUN npm install && \
    npx prisma generate && \
    npm prune --production

# 3) API entrypoint + CLI sources
COPY server/server.mjs   ./server.mjs
COPY server/mcguire-step-cli ./mcguire-step-cli

############################################################
#  Build the STEP→JSON CLI
############################################################
RUN mkdir -p mcguire-step-cli/build && \
    cd mcguire-step-cli/build && \
    cmake .. -DCMAKE_PREFIX_PATH=/usr/local && \
    cmake --build . --config Release && \
    cp mcguire_step_cli /usr/local/bin

############################################################
#  Run the Express server
############################################################
EXPOSE 3001
CMD ["node","server.mjs"]
