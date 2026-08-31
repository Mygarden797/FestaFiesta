import { mkdir, readFile, writeFile } from "node:fs/promises";
import { dirname, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const toolDir = dirname(fileURLToPath(import.meta.url));
const rootDir = resolve(toolDir, "..");
const generatedDir = resolve(rootDir, "Generated");
const outputPath = resolve(rootDir, "Notion", "FF_PrepBuild_LightTeal_VisualReview.html");

const catalog = await readFile(
  resolve(generatedDir, "FF_PrepBuild_CatalogOpen_NotionPreview_v1.jpg"),
);
const placing = await readFile(
  resolve(generatedDir, "FF_PrepBuild_Placing_NotionPreview_v1.jpg"),
);

const catalogData = `data:image/jpeg;base64,${catalog.toString("base64")}`;
const placingData = `data:image/jpeg;base64,${placing.toString("base64")}`;

const html = `<!doctype html>
<html lang="ko">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width,initial-scale=1">
  <title>시설 배치 UI · LightTeal Visual Review</title>
  <style>
    :root { --paper:#f7f8f6; --sunken:#eef2ef; --ink:#24332e; --muted:#6f7b75; --teal:#527c6b; --deep:#315747; --line:#ccd5cf; --amber:#e39a22; }
    * { box-sizing:border-box; }
    body { margin:0; color:var(--ink); background:linear-gradient(160deg,#e8f1ed 0%,#f7f8f6 48%,#edf1ee 100%); font-family:Pretendard,"Noto Sans KR",system-ui,sans-serif; }
    main { width:min(1180px,calc(100% - 32px)); margin:0 auto; padding:40px 0 48px; }
    header { display:grid; grid-template-columns:1fr auto; gap:24px; align-items:end; margin-bottom:24px; }
    .eyebrow { color:var(--teal); font-weight:800; letter-spacing:.12em; font-size:12px; text-transform:uppercase; }
    h1 { margin:8px 0 8px; font-size:clamp(28px,4vw,48px); line-height:1.08; letter-spacing:-.04em; }
    .lead { margin:0; color:var(--muted); line-height:1.65; max-width:760px; }
    .badge { padding:9px 13px; border:1px solid #9bb5aa; border-radius:999px; color:var(--deep); background:#eaf4ef; font-weight:800; font-size:12px; white-space:nowrap; }
    .grid { display:grid; grid-template-columns:repeat(2,minmax(0,1fr)); gap:18px; }
    .card { overflow:hidden; border:1px solid var(--line); border-radius:16px; background:rgba(247,248,246,.94); box-shadow:0 16px 46px rgba(36,51,46,.12); }
    .card img { display:block; width:100%; aspect-ratio:16/9; object-fit:cover; background:#dfe8e4; }
    .body { padding:18px 18px 20px; }
    .step { display:inline-grid; place-items:center; width:28px; height:28px; margin-right:8px; color:white; border-radius:50%; background:var(--teal); font-size:13px; font-weight:900; }
    h2 { display:inline; margin:0; font-size:20px; letter-spacing:-.02em; }
    .body p { margin:12px 0 0; color:var(--muted); line-height:1.6; font-size:14px; }
    .decision { margin-top:18px; display:grid; grid-template-columns:repeat(3,1fr); gap:1px; overflow:hidden; border:1px solid var(--line); border-radius:12px; background:var(--line); }
    .decision div { padding:15px; background:var(--paper); }
    .decision strong { display:block; color:var(--deep); font-size:13px; margin-bottom:5px; }
    .decision span { color:var(--muted); font-size:12px; line-height:1.5; }
    footer { margin-top:18px; padding:16px 18px; border:1px solid #aec3ba; border-radius:12px; background:#eaf4ef; color:var(--deep); font-size:13px; line-height:1.65; }
    footer b { color:var(--ink); }
    @media (max-width:760px) { header,.grid { grid-template-columns:1fr; } .badge { justify-self:start; } .decision { grid-template-columns:1fr; } }
  </style>
</head>
<body>
  <main>
    <header>
      <div>
        <div class="eyebrow">Festa Fiesta · Prep HUD</div>
        <h1>시설 배치 UI · LightTeal</h1>
        <p class="lead">종태님 HUD의 밝고 차분한 톤을 이어 가며, 탐색할 때는 정보가 충분하고 배치할 때는 월드가 주인공이 되도록 두 단계로 설계했습니다.</p>
      </div>
      <div class="badge">UMG PRODUCTION DIRECTION</div>
    </header>
    <section class="grid">
      <article class="card">
        <img src="${catalogData}" alt="시설 카탈로그 열림 상태 시안">
        <div class="body"><span class="step">1</span><h2>찾는 순간 · Catalog Open</h2><p>하단 도킹 패널에서 카테고리, 검색, 보유 골드, 시설 카드와 가격 상태를 한 흐름으로 확인합니다. 화면 전체를 어둡게 덮지 않아 월드 맥락을 유지합니다.</p></div>
      </article>
      <article class="card">
        <img src="${placingData}" alt="시설 배치 중 상태 시안">
        <div class="body"><span class="step">2</span><h2>놓는 순간 · Placing</h2><p>카드를 선택하면 목록을 축약하고 배치 프리뷰와 회전·확정·취소에 집중합니다. 동일한 팔레트와 하단 기준선을 유지해 상태 전환이 자연스럽습니다.</p></div>
      </article>
    </section>
    <section class="decision" aria-label="핵심 결정">
      <div><strong>런타임 UI</strong><span>입력·포커스·DPI·Cook을 한 경로로 관리하는 UMG</span></div>
      <div><strong>성능</strong><span>TileView 가상화, 비동기 썸네일, 이벤트 기반 갱신</span></div>
      <div><strong>협업 안전</strong><span>기존 파일 0건 수정, 연동 지점은 소유자 검토안으로 분리</span></div>
    </section>
    <footer><b>주의:</b> 이 이미지는 디자인 합의용 콘셉트입니다. 실제 시설명, 가격, 버튼 힌트, 상태 문구와 아이콘은 UMG에서 프로젝트 폰트·팔레트·현지화 규칙으로 다시 구성합니다.</footer>
  </main>
</body>
</html>`;

await mkdir(dirname(outputPath), { recursive: true });
await writeFile(outputPath, html, "utf8");
console.log(outputPath);
