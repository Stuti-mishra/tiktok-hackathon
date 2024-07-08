# Use an official Alpine as a parent image
FROM alpine:3.18

# Install dependencies
RUN apk update && apk add --no-cache \
    build-base \
    cmake \
    git \
    boost-dev \
    openssl-dev \
    opencv-dev \
    asio-dev 

# Clone and build Crow
RUN git clone https://github.com/CrowCpp/Crow.git /opt/crow \
    && cd /opt/crow \
    && mkdir build \
    && cd build \
    && cmake -DBUILD_EXAMPLES=OFF .. \
    && make \
    && make install

# Create a working directory
WORKDIR /app

# Copy the current directory contents into the container at /app
COPY . /app

# Compile the C++ application
RUN make

# Make port 8080 available to the world outside this container
EXPOSE 8002

# Run the C++ application
CMD ["./app"]
