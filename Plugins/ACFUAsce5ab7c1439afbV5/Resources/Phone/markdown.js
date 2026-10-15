(() => {
  const escape = value => String(value).replace(/[&<>"']/g, c => ({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  const parser = new marked.Marked({
    gfm: true,
    walkTokens(token) {
      if (token.type === 'text' && token.escaped) token.escaped = false;
    },
    renderer: {
      html({text}) { return escape(text); },
      image({text}) { return escape(text); },
      link({href, tokens}) {
        const label = this.parser.parseInline(tokens);
        let url;
        try { url = new URL(href); } catch { return label; }
        if (url.protocol !== 'https:' && url.protocol !== 'http:' && url.protocol !== 'mailto:') return label;
        return '<a href="' + escape(url.href) + '" rel="noreferrer noopener">' + label + '</a>';
      },
      table(token) {
        return '<div class="mdtable">' + marked.Renderer.prototype.table.call(this, token) + '</div>';
      }
    }
  });
  const safe = {
    ALLOWED_TAGS: ['p','h1','h2','h3','h4','h5','h6','strong','em','del','code','pre','ul','ol','li','blockquote','hr','br','a','div','table','thead','tbody','tr','th','td','input'],
    ALLOWED_ATTR: ['href','rel','class','align','start','type','disabled','checked'],
    ALLOW_DATA_ATTR: false,
    ALLOW_ARIA_ATTR: false
  };
  const cache = [];
  window.phoneMarkdown = {
    render(text, index = 0) {
      if (cache[index]?.text === text) return cache[index].html;
      const html = DOMPurify.sanitize(parser.parse(text), safe);
      cache[index] = {text, html};
      return html;
    },
    resize(length) { cache.length = length; }
  };
  const style = document.createElement('style');
  style.textContent = '.markdown{white-space:normal;overflow-wrap:anywhere;min-width:0}.markdown p{margin:.65em 0}.markdown h1,.markdown h2,.markdown h3,.markdown h4,.markdown h5,.markdown h6{line-height:1.3;margin:1em 0 .5em}.markdown h1{font-size:1.4em}.markdown h2{font-size:1.25em}.markdown h3,.markdown h4,.markdown h5,.markdown h6{font-size:1.1em}.markdown h1,.markdown h2,.markdown h3,.markdown h4,.markdown h5,.markdown h6{color:#c9c2a6}.markdown>:first-child{margin-top:0}.markdown>:last-child{margin-bottom:0}.markdown ul,.markdown ol{padding-left:1.5em;margin:.65em 0}.markdown li{margin:.25em 0}.markdown li>ul,.markdown li>ol{margin:.25em 0}.markdown blockquote{border-left:3px solid #6b675a;margin:.65em 0;padding-left:12px;color:#b9bec7}.markdown a{color:#f2f2f5;text-decoration-color:#b6b09a}.markdown code{font: .9em ui-monospace,SFMono-Regular,Consolas,monospace;color:#c9c2a6;background:#15161b;border-radius:3px;padding:1px 4px}.markdown pre{background:#15161b;border-radius:6px;padding:10px;overflow-x:auto;white-space:pre;word-break:normal;max-width:100%}.markdown pre code{background:none;padding:0}.markdown hr{border:0;border-top:1px solid #454650;margin:1em 0}.mdtable{max-width:100%;overflow-x:auto;margin:.65em 0}.mdtable table{border-collapse:collapse;min-width:100%;width:max-content;word-break:normal}.mdtable th,.mdtable td{border:1px solid #454650;padding:7px 10px}.mdtable th{background:#2a2b33}.markdown input[type=checkbox]{margin-right:5px}';
  document.head.appendChild(style);
})();
