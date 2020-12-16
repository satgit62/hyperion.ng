Compile
--------
sudo ./bin/scripts/docker-compile.sh -t amd64 -b Debug -p false

Execution
---------

sudo docker run  -t -i --device=/dev/ttyACM0 -v "/home/thomas/hyperion.ng-lordgrey/build:/hyperion:rw" hyperionproject/hyperion-ci:amd64 /bin/bash -c "ip -4 -o address && cd /hyperion && ./bin/hyperiond -d -u ada"


Help
---------

sudo docker run -v /dev:/dev -v "build:/hyperion:rw" hyperionproject/hyperion-ci:amd64 /bin/bash -c "ls -ltr /dev/tty*"

