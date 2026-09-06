FROM node:18-alpine

WORKDIR /app

# Install mysql2 for direct secure internal database communication
RUN npm init -y && npm install mysql2

COPY server.js ./

EXPOSE 80

CMD ["node", "server.js"]
