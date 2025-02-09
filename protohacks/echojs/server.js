import { createServer } from "net";

createServer((s) => s.on("data", s.write)).listen(process.env.PORT || 3000);
