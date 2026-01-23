/** @type {import('next').NextConfig} */
const nextConfig = {
  output: 'export',
  basePath: '/zap-cpp',
  assetPrefix: '/zap-cpp/',
  images: {
    unoptimized: true,
  },
  trailingSlash: true,
}

export default nextConfig
