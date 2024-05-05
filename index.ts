import fs from 'node:fs/promises';
import path from 'node:path';

const serverRoot = "share";

const server = Bun.serve({
    port: 8080,
    hostname: "0.0.0.0",
    async fetch(req, server) {
        const reqPath = new URL(req.url).pathname;
        const fullPath = path.join(serverRoot, reqPath);
        const stats = await fs.stat(fullPath);

        if (stats.isFile()) {
            if (req.method === "GET") {
                return new Response(Bun.file(fullPath));
            } else {
                return new Response("Method Not Allowed", { status: 405 });
            }
        } else if (stats.isDirectory()) {
            if (req.method === "GET") {
                return new Response(Bun.file('./index.html'));
            } else if (req.method === "POST") {
                return Response.json([
                    {
                        name: "..",
                        path: path.normalize(path.join(reqPath, "..")),
                        type: 'directory'
                    }, ...(await fs.readdir(fullPath, { withFileTypes: true })).map(dirent => ({
                        name: dirent.name,
                        path: path.join(reqPath, dirent.name),
                        type: dirent.isDirectory() ? 'directory' : 'file'
                    }))
                ]);
            } else {
                return new Response("Method Not Allowed", { status: 405 });
            }
        } else {
            return new Response("Not Found", { status: 404 });
        }
    },
    error(error) {
        return new Response(error.message, { status: 400 });
    }
});

console.log(`Listening on ${server.url}`);