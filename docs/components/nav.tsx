'use client'

import Link from 'next/link'
import { usePathname } from 'next/navigation'
import { cn } from '@/lib/utils'
import { docsConfig } from '@/lib/docs'

export function Nav() {
  const pathname = usePathname()

  return (
    <nav className="w-64 shrink-0 border-r border-border">
      <div className="sticky top-0 h-screen overflow-y-auto p-6">
        <Link href="/" className="flex items-center gap-2 mb-8">
          <span className="font-bold text-xl">ZAP C++</span>
        </Link>

        <div className="space-y-6">
          {docsConfig.map((section) => (
            <div key={section.slug}>
              <h4 className="font-semibold text-sm text-muted-foreground mb-2">
                {section.title}
              </h4>
              <ul className="space-y-1">
                {section.items.map((item) => (
                  <li key={item.slug}>
                    <Link
                      href={item.href}
                      className={cn(
                        'block py-1.5 px-3 text-sm rounded-md transition-colors',
                        pathname === item.href
                          ? 'bg-accent text-accent-foreground font-medium'
                          : 'text-muted-foreground hover:text-foreground hover:bg-accent/50'
                      )}
                    >
                      {item.title}
                    </Link>
                  </li>
                ))}
              </ul>
            </div>
          ))}
        </div>

        <div className="mt-8 pt-6 border-t border-border">
          <a
            href="https://github.com/zap-protocol/zap-cpp"
            target="_blank"
            rel="noopener noreferrer"
            className="text-sm text-muted-foreground hover:text-foreground"
          >
            GitHub
          </a>
        </div>
      </div>
    </nav>
  )
}
