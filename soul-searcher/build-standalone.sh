#!/bin/bash
# Build a single standalone HTML file from the React component

GAME_JSX=$(cat soul-searcher/src/SoulSearcher.jsx | sed '1d')  # strip import line

cat > soul-searcher-standalone.html << 'HEADER'
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Soul Searcher — Inkborne</title>
<style>
  * { margin: 0; padding: 0; box-sizing: border-box; }
  body { background: #121212; overflow: hidden; }
</style>
<script crossorigin src="https://unpkg.com/react@18/umd/react.production.min.js"></script>
<script crossorigin src="https://unpkg.com/react-dom@18/umd/react-dom.production.min.js"></script>
<script src="https://unpkg.com/@babel/standalone/babel.min.js"></script>
</head>
<body>
<div id="root"></div>
<script type="text/babel">
const { useState, useEffect, useCallback, useRef } = React;
HEADER

echo "$GAME_JSX" >> soul-searcher-standalone.html

cat >> soul-searcher-standalone.html << 'FOOTER'

const root = ReactDOM.createRoot(document.getElementById('root'));
root.render(React.createElement(SoulSearcher));
</script>
</body>
</html>
FOOTER

echo "Built: soul-searcher-standalone.html"
