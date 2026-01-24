import './globals.css'
import { RootProvider } from 'fumadocs-ui/provider'
import type { Metadata } from 'next'
import type { ReactNode } from 'react'

export const metadata: Metadata = {
  title: {
    template: '%s | ZAP C++',
    default: 'ZAP C++ Documentation',
  },
  description: 'C++ bindings for ZAP - Zero-Copy App Proto for AI agent communication',
}

export default function RootLayout({ children }: { children: ReactNode }) {
  return (
    <html lang="en" suppressHydrationWarning>
      <body>
        <RootProvider>{children}</RootProvider>
      </body>
    </html>
  )
}
