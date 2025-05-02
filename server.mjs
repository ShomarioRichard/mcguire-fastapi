// server/server.mjs
import express from 'express';
import cors from 'cors';
import bcrypt from 'bcrypt';
import { PrismaClient } from '@prisma/client';

const app = express();
const prisma = new PrismaClient();

app.use(express.json());
app.use(cors());

app.use((req, res, next) => {
  console.log(`[${req.method}] ${req.path}`);
  next();
});

app.post('/api/register', async (req, res) => {
  try {
    console.log("📥 Register Request Body:", req.body);
    const { name, email, password } = req.body;

    if (!name || !email || !password) {
      console.warn("⚠️ Missing field(s) in register request");
      return res.status(400).send('All fields required.');
    }

    const existingUser = await prisma.user.findUnique({ where: { email } });
    if (existingUser) {
      console.warn(`⚠️ User already exists: ${email}`);
      return res.status(409).send('User already exists.');
    }

    const hashed = await bcrypt.hash(password, 10);

    await prisma.user.create({
      data: { name, email, passwordHash: hashed }
    });

    console.log(`✅ Registered user: ${email}`);
    res.status(200).send('Registered successfully');
  } catch (err) {
    console.error("❌ Registration failed:", err);
    res.status(500).send('Server error.');
  }
});

app.post('/api/login', async (req, res) => {
  try {
    console.log("📥 Login Request Body:", req.body);
    const { email, password } = req.body;

    if (!email || !password) {
      return res.status(400).send('Email and password are required.');
    }

    const user = await prisma.user.findUnique({ where: { email } });
    if (!user) {
      return res.status(401).send('Invalid credentials.');
    }

    const match = await bcrypt.compare(password, user.passwordHash);
    if (!match) {
      return res.status(401).send('Invalid credentials.');
    }

    return res.status(200).send('Login successful');
  } catch (err) {
    console.error("❌ Login failed:", err);
    res.status(500).send('Server error.');
  }
});

// Global error logging
app.use((err, req, res, next) => {
  console.error("💥 Unhandled error:", err);
  res.status(500).json({ error: 'Internal Server Error' });
});

// Global exception handlers
process.on('uncaughtException', (err) => {
  console.error("🔥 Uncaught Exception:", err);
});

process.on('unhandledRejection', (err) => {
  console.error("🔥 Unhandled Rejection:", err);
});

const port = process.env.PORT || 3001;

try {
  app.listen(port, () => {
    console.log(`🚀 Server running on port ${port}`);
    console.log(`🌎 Environment: ${process.env.NODE_ENV || 'development'}`);
  });
} catch (err) {
  console.error("❌ Failed to start server:", err);
}

console.log("🌐 Using DATABASE_URL:", process.env.DATABASE_URL);

