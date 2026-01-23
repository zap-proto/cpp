import { Nav } from './nav'

interface DocsLayoutProps {
  children: React.ReactNode
}

export function DocsLayout({ children }: DocsLayoutProps) {
  return (
    <div className="flex min-h-screen">
      <Nav />
      <main className="flex-1 p-8 max-w-4xl">
        <article className="prose dark:prose-invert">
          {children}
        </article>
      </main>
    </div>
  )
}
