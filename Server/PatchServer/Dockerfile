FROM node:18-alpine

WORKDIR /app

COPY server.js ./
COPY patch_data ./patch_data

EXPOSE 80

CMD ["node", "server.js"]
