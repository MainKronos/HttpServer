import fs from 'node:fs/promises';
import path from 'node:path';

const serverRoot = "public";

const server = Bun.serve({
    port: 8080,
    hostname: "10.58.64.209",
    async fetch(req, server) {
        const reqPath = decodeURI(new URL(req.url).pathname);
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
                        type: 'directory',
                        size: null,
                        mtime: null
                    }, ...await Promise.all((await fs.readdir(fullPath)).map(async (file) => {
                        const stats = await fs.stat(path.join(fullPath, file));
                        return {
                            name: file,
                            path: path.join(reqPath, file),
                            type: stats.isDirectory() ? 'directory' : 'file',
                            size: stats.size,
                            mtime: stats.mtime
                        };
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