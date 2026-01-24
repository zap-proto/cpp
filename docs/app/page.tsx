import Link from 'next/link'

export default function HomePage() {
  return (
    <main className="flex min-h-screen flex-col items-center justify-center p-8 text-center">
      <h1 className="mb-4 text-4xl font-bold">ZAP C++</h1>
      <p className="mb-8 max-w-2xl text-lg text-fd-muted-foreground">
        High-performance serialization and RPC framework based on Cap&apos;n Proto.
        Zero-copy design, schema evolution, and capability-based security.
      </p>
      <div className="flex gap-4">
        <Link
          href="/docs"
          className="rounded-lg bg-fd-primary px-6 py-3 font-medium text-fd-primary-foreground transition-colors hover:bg-fd-primary/90"
        >
          Get Started
        </Link>
        <Link
          href="https://github.com/zap-protocol/zap-cpp"
          className="rounded-lg border border-fd-border px-6 py-3 font-medium transition-colors hover:bg-fd-accent"
        >
          GitHub
        </Link>
      </div>
    </main>
  )
}
