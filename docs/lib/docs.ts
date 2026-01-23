export interface DocSection {
  title: string
  slug: string
  items: DocItem[]
}

export interface DocItem {
  title: string
  slug: string
  href: string
}

export const docsConfig: DocSection[] = [
  {
    title: 'Getting Started',
    slug: 'getting-started',
    items: [
      { title: 'Introduction', slug: 'introduction', href: '/docs/introduction' },
      { title: 'Installation', slug: 'installation', href: '/docs/installation' },
      { title: 'Building', slug: 'building', href: '/docs/building' },
    ],
  },
  {
    title: 'Core Concepts',
    slug: 'core-concepts',
    items: [
      { title: 'Schema Language', slug: 'schema', href: '/docs/schema' },
      { title: 'Serialization', slug: 'serialization', href: '/docs/serialization' },
      { title: 'RPC System', slug: 'rpc', href: '/docs/rpc' },
    ],
  },
  {
    title: 'API Reference',
    slug: 'api-reference',
    items: [
      { title: 'C++ API', slug: 'api', href: '/docs/api' },
      { title: 'KJ Library', slug: 'kj', href: '/docs/kj' },
    ],
  },
  {
    title: 'Examples',
    slug: 'examples',
    items: [
      { title: 'Basic Usage', slug: 'examples', href: '/docs/examples' },
      { title: 'RPC Examples', slug: 'rpc-examples', href: '/docs/rpc-examples' },
    ],
  },
]
