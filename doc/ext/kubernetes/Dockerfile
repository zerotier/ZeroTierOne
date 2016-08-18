FROM node:4.4
EXPOSE 8080
COPY server.js .
COPY zerotier .
COPY zerotier-cli .
COPY entrypoint.sh .
COPY .zerotierCliSettings <settings path>?>
CMD node server.js