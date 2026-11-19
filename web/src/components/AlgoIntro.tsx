export function AlgoIntro({ title, summary, complexity }: { title: string; summary: string; complexity: [string, string][] }) {
  return (
    <div className="intro">
      <div>
        <h2>{title}</h2>
        <p>{summary}</p>
      </div>
      <dl className="facts">
        {complexity.map(([k, v]) => (
          <div key={k}><dt>{k}</dt><dd>{v}</dd></div>
        ))}
      </dl>
    </div>
  );
}
