const path = require('path');

module.exports = {
  mode: 'development',
  entry: './src/index.ts',
  output: {
    filename: 'bundle.js',
    path: path.resolve(__dirname, 'dist'),
  },
  devtool: 'inline-source-map',
  devServer: {
    static: './dist',
    allowedHosts: [
      'localhost', // localhostからのアクセスを許可
      '127.0.0.1', // 127.0.0.1からのアクセスを許可
      '.ngrok.io', // ngrok などを使用している場合
      'sxcstr.sxclij.com', // 特定のドメインを許可する場合
    ],
  },
  module: {
    rules: [
      {
        test: /\.tsx?$/,
        use: 'ts-loader',
        exclude: /node_modules/,
      },
    ],
    
  },
   resolve: {
    extensions: ['.tsx', '.ts', '.js'],
  },
};