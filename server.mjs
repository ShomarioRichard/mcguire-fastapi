// server/server.mjs
import express from 'express';
import cors from 'cors';
import bcrypt from 'bcrypt';
import multer from 'multer';
import fs from 'fs';
import path from 'path';
import { execFile } from 'child_process';
import { PrismaClient } from '@prisma/client';

const app = express();
const prisma = new PrismaClient();
const upload = multer({ dest: 'uploads/' });

app.use(express.json());
app.use(cors());

app.use((req, res, next) => {
  console.log(`[${req.method}] ${req.path}`);
  next();
});

// -- REGISTER --
app.post('/api/register', async (req, res) => {
  try {
    const { name, email, password } = req.body;
    if (!name || !email || !password) return res.status(400).send('All fields required.');

    const existingUser = await prisma.user.findUnique({ where: { email } });
    if (existingUser) return res.status(409).send('User already exists.');

    const hashed = await bcrypt.hash(password, 10);
    await prisma.user.create({ data: { name, email, passwordHash: hashed } });

    res.status(200).send('Registered successfully');
  } catch (err) {
    console.error("❌ Registration failed:", err);
    res.status(500).send('Server error.');
  }
});

// -- LOGIN --
app.post('/api/login', async (req, res) => {
  try {
    const { email, password } = req.body;
    if (!email || !password) return res.status(400).send('Email and password are required.');

    const user = await prisma.user.findUnique({ where: { email } });
    if (!user || !(await bcrypt.compare(password, user.passwordHash)))
      return res.status(401).send('Invalid credentials.');

    res.status(200).send('Login successful');
  } catch (err) {
    console.error("❌ Login failed:", err);
    res.status(500).send('Server error.');
  }
});

// -- STEP CONVERSION ROUTE --
app.post('/api/convert-step', upload.single('file'), (req, res) => {
  const inputPath = req.file.path;
  const outputPath = `${inputPath}.json`;
  const cliPath = path.resolve('./mcguire-step-cli/build/mcguire_step_cli');

  execFile(cliPath, [inputPath, outputPath], (error, stdout, stderr) => {
    if (error) {
      console.error('❌ CLI error:', stderr);
      return res.status(500).send('STEP conversion failed.');
    }

    fs.readFile(outputPath, 'utf8', (err, data) => {
      fs.unlinkSync(inputPath);
      fs.unlinkSync(outputPath);

      if (err) {
        console.error('❌ Read error:', err);
        return res.status(500).send('Failed to read JSON output.');
      }

      try {
        const json = JSON.parse(data);
        return res.status(200).json(json);
      } catch (parseErr) {
        console.error('❌ JSON parse error:', parseErr);
        return res.status(500).send('Invalid JSON output.');
      }
    });
  });
});

// -- ERROR LOGGING --
app.use((err, req, res, next) => {
  console.error("💥 Unhandled error:", err);
  res.status(500).json({ error: 'Internal Server Error' });
});

process.on('uncaughtException', (err) => console.error("🔥 Uncaught Exception:", err));
process.on('unhandledRejection', (err) => console.error("🔥 Unhandled Rejection:", err));

const port = process.env.PORT || 3001;
app.listen(port, () => {
  console.log(`🚀 Server running on port ${port}`);
  console.log(`🌎 Environment: ${process.env.NODE_ENV || 'development'}`);
});

console.log("🌐 Using DATABASE_URL:", process.env.DATABASE_URL);
