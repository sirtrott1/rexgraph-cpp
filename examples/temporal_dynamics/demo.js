'use strict';
const records=DEMO.records, chapters=[...document.querySelectorAll('#chapters button')].map(b=>b.dataset.chapter);
const databases=records.filter(r=>r.chapter==='database');
let cutoff=0,chapter='introduction',nativeToken=null,queryEpoch=0;
const categories={1:'Operation',2:'Intervention',3:'Closure'};
const subtypes={1:'Startup',2:'Alarm',3:'Baseline',4:'Inspection',5:'Repair',6:'Episode',7:'Declared closure'};
const $=id=>document.getElementById(id);
const esc=x=>String(x??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const exact=x=>`<span class="exact">${esc(x??'Absent')}</span>`;
const row=(kind,seq=cutoff)=>records.find(r=>r.chapter===kind&&r.sequence===seq);
const reference=kind=>records.find(r=>r.chapter===kind);
const table=(heads,rows,cls='')=>`<div class="table-wrap"><table class="${cls}"><thead><tr>${heads.map(x=>`<th scope="col">${esc(x)}</th>`).join('')}</tr></thead><tbody>${rows.map(r=>`<tr>${r.map(c=>`<td>${c}</td>`).join('')}</tr>`).join('')}</tbody></table></div>`;
const mat=(a,labels=[],cols=[])=>!a.length||!a[0]?.length?'<p class="empty">This declared carrier has no columns at this publication.</p>':table(['Coordinate',...a[0].map((_,i)=>cols[i]??String(i))],a.map((r,i)=>[esc(labels[i]??String(i)),...r.map(exact)]),'matrix');
const card=(value,label)=>`<div class="card"><span class="number">${esc(value)}</span><span class="label">${esc(label)}</span></div>`;
const details=(title,body)=>`<details><summary>${esc(title)}</summary><div>${body}</div></details>`;
const code=x=>`<pre><code>${esc(x)}</code></pre>`;
const eq=x=>`<div class="equation">${x}</div>`;
const note=x=>`<div class="notice">${x}</div>`;
const num=x=>{const [a,b='1']=String(x).split('/');return Number(a)/Number(b);}; // Display geometry only.

function timeline(d){
  const max=Math.max(10,...d.events.map(e=>num(e.end))),w=900,left=170,right=80,scale=(w-left-right)/max;
  const h=70+d.events.length*48;
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="Time spans retained at publication ${d.sequence}"><text x="170" y="25">Valid time in minutes</text>`;
  for(let i=0;i<=4;i++){const t=max*i/4,x=left+t*scale;s+=`<line class="grid" x1="${x}" x2="${x}" y1="35" y2="${h-28}"/><text class="muted" x="${x}" y="${h-5}" text-anchor="middle">${t}</text>`;}
  d.events.forEach((e,i)=>{const y=53+i*48,a=left+num(e.start)*scale,b=left+num(e.end)*scale;s+=`<text x="16" y="${y+6}">${esc(subtypes[e.subtype])}</text><rect class="${e.first_known_or_corrected===cutoff?'selected':'shape'}" x="${a}" y="${y-12}" width="${Math.max(2,b-a)}" height="27" rx="2"/><text x="${a+7}" y="${y+6}">[${esc(e.start)}, ${esc(e.end)})</text>`;});
  return `<div class="chart">${s}</svg></div><p class="caption">Only relations retained at the selected publication are drawn. A highlighted span was introduced or corrected in that publication. Span length controls pixel width; it does not assign a chain grade.</p>`;
}

function historyPlot(){
  const data=records.filter(r=>r.chapter==='query'&&r.sequence<=cutoff),w=880,h=210,left=65,bottom=165;
  const max=Math.max(1,...data.map(r=>num(r.reading_sum??0))),space=(w-left-40)/Math.max(data.length,1);
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="Exact reading sums through selected publication"><line class="axis" x1="${left}" x2="${w-30}" y1="${bottom}" y2="${bottom}"/>`;
  for(const r of data){const x=left+space*r.sequence+space*.2,bh=120*num(r.reading_sum??0)/max;s+=`<rect class="bar" x="${x}" y="${bottom-bh}" width="${space*.6}" height="${bh}"/><text x="${x+space*.3}" y="${bottom-bh-8}" text-anchor="middle">${esc(r.reading_sum??'Absent')}</text><text x="${x+space*.3}" y="${bottom+26}" text-anchor="middle">Publication ${r.sequence}</text>`;}
  return `<div class="chart">${s}</svg></div><p class="caption">Native query sums for [5, 15), through the selected cutoff. Bars are a display of exact retained results. A correction changes the later result and preserves the earlier result.</p>`;
}

function graph(d){
  const assets=['pump','valve','controller'],w=940,h=Math.max(360,90+d.events.length*62);
  let s=`<svg viewBox="0 0 ${w} ${h}" role="img" aria-label="Typed relations and their explicitly retained asset roles">`;
  assets.forEach((a,i)=>{const y=50+i*100;s+=`<rect class="shape" x="740" y="${y}" width="160" height="42" rx="2"/><text x="820" y="${y+27}" text-anchor="middle">${a}</text>`;});
  d.events.forEach((e,i)=>{const y=20+i*62;const counts=new Map();e.assets.forEach(a=>counts.set(a,(counts.get(a)||0)+1));
    for(const [a,n] of counts){const k=assets.indexOf(a.replace(/^!/,''));if(k<0)continue;const target=71+k*100;s+=`<path class="link" ${a.startsWith('!')?'stroke-dasharray="5 5"':''} d="M 465 ${y+23} C 580 ${y+23}, 625 ${target}, 740 ${target}"/>`;if(n>1)s+=`<text x="505" y="${y+15}">${n} occurrences</text>`;}
    s+=`<rect class="shape" x="20" y="${y}" width="445" height="46" rx="2"/><text x="35" y="${y+29}">${esc(subtypes[e.subtype])}: [${esc(e.start)}, ${esc(e.end)})</text>`;
  });
  return `<div class="chart">${s}</svg></div><p class="caption">Rectangles are primary event relations and named asset subjects. Curves show the declared asset role. Dashed curves retain inactive occurrences. Repeated roles remain separate occurrences in native storage. This drawing introduces no causal edge or inferred higher relation.</p>`;
}

function queryResult(q){
  return `<div class="cards">${card(q.selected,'Selected primary relations')}${card(q.reading_sum??'Absent','Exact reading sum')}${card(q.numeric_contributions,'Present scalar contributions')}</div>`+
    table(['Selected event','Native key'],q.events.map((x,i)=>[esc(x),exact(q.keys[i])]))+
    table(['Category','Original members','Count','Reading sum','Present readings'],q.groups.map(g=>[esc(categories[g.category]),esc(g.member_events.join(', ')),exact(g.count),exact(g.reading_sum),String(g.numeric_contributions)]))+
    `<p class="caption">Source namespace ${q.namespace}, publication ${q.source_sequence}, availability cutoff ${q.cutoff}. The query cache validates the complete native source and request. Group membership survives aggregation.</p>`;
}

const intro=()=>`<h1>Time is part of the relation</h1><p class="lead">Follow a cooling circuit incident through six native publications. Each event retains its time span, category, subtype, asset occurrences and exact tensor payload. Queries and field responses use the information available at the selected publication.</p>`+
  note('<strong>Two clocks answer different questions.</strong><p>Valid time says when an event happened. Publication order says when its data became available. A late inspection belongs to an earlier valid time span, but enters the database only when it is published.</p>')+
  `<ol class="steps"><li>Advance the publication selector and watch the time spans enter the database.</li><li>Query an overlap interval and inspect retained group membership.</li><li>Read the signed boundary, exact field response and Hodge sectors.</li><li>Inspect the finite change channels, their cross pairings and the ordered contact control.</li><li>Return to publication 0. Its native query and field result remain unchanged.</li></ol>`+
  `<h2>One retained source, declared views</h2><p>Event relations are primary. Time points and asset subjects are referenced through explicit roles. The typed database interpretation retains these roles and their payloads. A separate signed span realization maps the same event identities into a declared chain complex; each link names both native namespaces.</p>`+
  eq('Event relation → typed time and asset roles → declared span boundary → exact field action')+
  `<p>Time contributes a positive duration metric on grade one. Category and subtype remain payload declarations. Duration, category, grade, tensor shape and display position have distinct meanings.</p>`+
  `<h2>What this demonstrates</h2><div class="cards">${card('Retained time','Historical native queries')}${card('Exact changes','Finite attribution and interactions')}${card('Typed structure','Occurrences, roles and tensor support')}</div>`+
  `<p>The incident is synthetic. Its field is an explicitly declared mathematical observation of the data. It does not claim a physical cooling model, an inferred causal mechanism or a statistical significance test.</p>`;

const timelineView=()=>{const d=row('database');return `<h1>Time spans and publications</h1><p>At publication ${cutoff}, data is available through minute ${d.available_minute}. The database retains ${d.events.length} primary event relations. Select a later publication to admit later information, or an earlier one to recover its historical view.</p>`+timeline(d)+
  table(['Event','Category','Subtype','Valid span','Latest retained assertion','Reading','Asset occurrences','Operator role'],d.events.map(e=>[esc(e.event),esc(categories[e.category]),esc(subtypes[e.subtype]),exact(`[${e.start}, ${e.end})`),`Publication ${e.first_known_or_corrected}`,exact(e.reading),esc(e.assets.join(', ')),['Missing','Null','Present'][e.operator_state]]))+
  `<h2>How to read this publication</h2>`+
  (cutoff>=2?note('<strong>Late arrival:</strong> the inspection covers minutes [5, 15), but first becomes available at publication 2. Publications 0 and 1 do not contain it.'):'<p>Only accepted publications are included. The scenario has not yet admitted a late inspection.</p>')+
  (cutoff>=4?note('<strong>Correction:</strong> baseline keeps its native relation identity, while its reading and forcing become supported zero at publication 4. Earlier native files retain the value 2.'):'<p>Producer identities persist through the native update history. A correction will create a new publication.</p>')+
  graph(d)+`<p><a href="${esc(d.snapshot)}" download>Download this native database publication</a></p>`;};

const queryView=()=>`<h1>Query the native database</h1><p>The prepared query selects event spans that overlap [5, 15), sums present readings and groups results by the declared category. It uses native exact predicates and indexes. SQL is not involved.</p>`+
  eq('<span class="math">start &lt; 15 and end &gt; 5</span>')+queryResult(row('query'))+historyPlot()+
  `<h2>Run another interval</h2><form id="queryForm" class="form"><label>Interval start<input id="queryStart" value="5" inputmode="text" aria-label="Exact interval start"></label><label>Interval end<input id="queryEnd" value="15" inputmode="text" aria-label="Exact interval end"></label><button id="runQuery" type="submit" disabled>Run native query</button></form><p id="queryStatus" role="status">Open this presentation with serve.py to run new queries against the native database. The result above is a completed native query.</p><div id="queryResult"></div>`+
  details('Query syntax and standalone commands',`<p>Intervals are half open. Enter integers or exact fractions such as 1/2. The native consumer binds the selected file to its publication cutoff and rejects a later endpoint at an earlier cutoff.</p>`+
    code(`temporal_dynamics query database/publication-${cutoff}.rfcxi ${cutoff} 5 15 --exact`)+
    `<p>The generated notebook uses the native RFC query language:</p>`+code(`select 10\nwhere start < rational:15\nwhere end > rational:5\nsum reading\nfigure reading\nroles 13 retained\npage 0 1000`)+
    `<p>select names the TimeSpan assertion kind. where names declared payload factors. sum omits absent readings and includes supported zero. roles shows retained asset occurrences. The input file is named explicitly in each notebook cell.</p>`)+
  `<h2>Occurrences and nullable values</h2><p>Startup retains two pump occurrences and an inactive valve occurrence. The native role join retains their multiplicity. Operator references distinguish missing, null and present. The inspection's absent reading is not a zero reading.</p>`+
  table(['Join activity','Exact occurrence pairs'],[['All retained asset occurrences',String(row('database').retained_join_matches)],['Active asset occurrences',String(row('database').active_join_matches)]])+`<p class="caption">This is an ordered self join over a declared shared asset role, including diagonal assertion pairs. It is not a count of unique event pairs or evidence of causation.</p>`;

const structureView=()=>{const f=row('field'),b=row('binary');return `<h1>Boundary and composite binary</h1><p>An ordinary span has the declared boundary end minus start. The demonstration also retains an explicit cancelling pair, an inactive term and a supported zero term. Their exact aggregate is the same signed span, but their retained source structure differs.</p>`+
  eq('∂₁ [s, e] = [e] − [s]')+
  table(['Retained term','Exists','Negative','Magnitude','Realized contribution'],[['Start','1','1','1','−1'],['End','1','0','1','+1'],['Start audit term','1','0','2','+2'],['Start audit term','1','1','2','−2'],['Inactive end term','0','0','7','0'],['Supported zero at end','1','0','0','0']])+`<p class="caption">The additional audit terms are declared synthetic source occurrences. They are not inferred from a CSV row or fabricated by the visualization.</p>`+
  `<div class="cards">${card(b.occurrences,'Retained grade one occurrences')}${card(b.inactive_retained,'Inactive occurrences retained')}${card(b.active_zero,'Active supported zero terms')}</div>`+
  `<p>The packed CPU program loads the existence and orientation planes, then evaluates e AND o and e AND NOT o. It reports ${b.active_negative} active negative and ${b.active_positive} active positive occurrences. These masks do not flatten signs, activity and coefficient values into one Boolean.</p>`+
  `<h2>Exact boundary matrix</h2>`+mat(f.boundary_matrix,f.time_coordinates.map(x=>`Minute ${x}`),f.relations)+
  `<p class="caption">Rows are declared time point identities in native order. Columns are primary span identities in the separate signed realization. Reordering the display does not change their identity.</p>`+
  details('Declared database and chain identities',table(['Minute','Database namespace','Database subject','Field namespace','Chain identity'],f.declared_time_links.map(x=>[exact(x.minute),String(x.database_namespace),String(x.database_key),String(x.field_namespace),String(x.chain_key)]))+`<p>Time points are named by their canonical rational minute in this scenario's one declared clock. The table explicitly links the actual database subjects to their separate chain identities.</p>`)+
  `<h2>Higher relation and the chain condition</h2>`+
  (cutoff===5?eq('∂₂ closure = startup + alarm + repair − episode<br>∂₁ ∂₂ closure = 0')+`<p>The chain check succeeds because the first three consecutive spans have exactly the boundary of the enclosing episode span. This higher relation is first asserted at publication 5. An overlapping interval alone does not create a face.</p>`+mat(f.higher_boundary,f.relations,['Closure']):`<p>No higher boundary has been asserted at this publication. The grade two carrier is explicitly declared and empty. Cycles in grade one are not automatically filled.</p>`)+
  `<p>The applicable chain interface is checked for every source publication before field execution.</p>`;};

const fieldView=()=>{const f=row('field');return `<h1>Field response and sectors</h1><p>The input forcing is a declared exact scalar on each ordinary span. The positive metric M₁ weights each relation by its duration; M₀ and M₂ are identity metrics on their own carriers. Native boundary and metric adjoint actions define the Hodge operator.</p>`+
  eq('L₁ = ∂₁<sup>†</sup> ∂₁ + ∂₂ ∂₂<sup>†</sup><br>∂₁<sup>†</sup> = M₁⁻¹ ∂₁ᵀ M₀<br>(I + L₁)x = f')+
  `<p class="caption">The dagger denotes the metric adjoint, following the paper. The displayed T denotes coordinate transpose; transpose and metric adjoint remain separate.</p>`+
  `<p>A prepared native solve retains the operator and its exact source authority. The complete residual is zero. A repeated action reuses the prepared solve and returns identical rational values.</p>`+
  table(['Primary span','Input f','Resolved x','Gradient part of f','Curl part of f','Harmonic part of f'],f.relations.map((r,i)=>[esc(r),...['forcing','response','gradient','curl','harmonic'].map(k=>exact(f[k][i]))]))+
  `<div class="cards">${card(f.sector_dimensions[0],'Gradient dimensions')}${card(f.sector_dimensions[1],'Curl dimensions')}${card(f.sector_dimensions[2],'Harmonic dimensions')}</div>`+
  `<p>The sectors decompose the input forcing, not the response column: f = gradient + curl + harmonic. Curl is absent until a higher boundary is asserted. The closing relation changes this operator sector without creating new grade one spans.</p>`+
  (cutoff>=2?`<p>The inspection overlaps other spans in valid time. Its distinct endpoints form a separate chain component in this declared realization. Intersections are not automatically split or filled.</p>`:'')+
  details('Native metric and operator tensors',`<h3>Duration metric M₁</h3>`+mat(f.metric,f.relations,f.relations)+`<h3>Hodge operator L₁</h3>`+mat(f.hodge_operator,f.relations,f.relations)+`<p>Response quadrance xᵀ M₁ x = ${exact(f.response_quadrance)}. Matrix materialization is a small diagnostic view; the library provides native retained boundary actions and prepared exact solvers.</p>`)+
  details('Measured preparation and reuse',table(['Measured action','Nanoseconds'],[['Preparation and durable publication',String(f.prepare_ns)],['First prepared action',String(f.cold_action_ns)],['Repeated prepared action',String(f.warm_action_ns)]])+`<p class="caption">These measurements describe this small demonstration run. They are not a general speed comparison, a hardware cache counter or a CPU versus GPU benchmark.</p>`);};

const changesView=()=>{const t=row('transition'),a=row('accumulation'),f=row('field');return `<h1>Evolution and attribution</h1><p>Every finite comparison names both original endpoints. The rectangular map J follows retained primary relation identities. Its rows for new target identities explicitly contain zero old contributions; equal dimensions or equal values do not supply a map. No inverse of this rectangular comparison is inferred.</p>`+
  eq('Δx = x′ − Jx<br>η = f′ − Kf<br>E<sub>L</sub> = L′J − JL')+
  (!t?`<p class="empty">Publication 0 is the initial endpoint. Advance one publication to inspect a finite update.</p>`:
    `<p>For the declared identity injections and λ = 1, Δx separates exactly into realization, source innovation and operator change. The realization term is zero in this example. The remaining channels use the new resolvent on the declared new carrier.</p>`+
    eq('Δx = (I + L′)⁻¹ η − (I + L′)⁻¹ E<sub>L</sub>x')+
    table(['Span','Source innovation channel','Operator change channel','Complete response change'],f.relations.map((r,i)=>[esc(r),exact(t.innovation[i]),exact(t.operator[i]),exact(t.delta[i])]))+
    `<h2>Complete channel pairing</h2>`+mat(t.channel_gram,['Realization','Innovation','Operator'],['Realization','Innovation','Operator'])+
    `<p>Sum every matrix entry to obtain Δxᵀ M₁′ Δx = ${exact(t.quadrance)}. The off diagonal entries are the exact interactions between channels. Keeping only the diagonal entries would lose this identity.</p>`+
    `<p>The finite rate divides this response change by ${exact(t.availability_elapsed_minutes)} minutes of publication availability time. It is a difference quotient over accepted publications, not an instantaneous physical derivative.</p>`+
    details('Declared rectangular comparison',mat(t.correspondence,f.relations,row('field',cutoff-1).relations)))+
  `<h2>Chronological accumulation</h2><p>The initial response and each accepted innovation are transported to the selected terminal carrier in chronological order. Their sum reconstructs the current response exactly. The terminal pairing below is an observation at publication ${cutoff}; it is not information available at an earlier publication.</p>`+
  mat(a.gram,['Initial',...Array.from({length:cutoff},(_,i)=>`Change ${i+1}`)],['Initial',...Array.from({length:cutoff},(_,i)=>`Change ${i+1}`)])+
  `<p>Complete terminal quadrance: ${exact(a.quadrance)}.</p>`+
  `<h2>Finite changes include mixed terms</h2><p>A separate two factor control verifies the full finite identity. It retains δB δA along with the two individual factor changes. The product need not commute.</p>`+
  eq('(B + δB)(A + δA) − BA = B δA + δB A + δB δA')+
  details('Exact mixed term',mat(reference('finite_interaction').interaction));};

const chronologyView=()=>{const c=row('chronology'),control=records.find(r=>r.chapter==='order_control');return `<h1>Ordered propagation</h1><p>The alarm and repair roles have explicit pulse transfer maps on the asset carrier (pump, valve, controller). The alarm transfers pump to valve. The later repair transfers valve to controller. Only accepted contacts are retained in each chronological prefix.</p>`+
  table(['Asset','Pulse at the selected publication'],c.assets.map((name,i)=>[esc(name),exact(c.state[i])]))+
  `<p>Accepted contact maps: ${c.accepted_contacts}. These maps are a declared observation rule. Asset sharing alone does not infer a transfer.</p>`+
  (cutoff>=3&&control?`<h2>Order changes the result</h2><p>The retained order reaches controller. The reversed order reaches valve. The reversed result is labelled as a counterfactual control at publication 3, when both maps are available.</p>`+table(['Asset','Retained order','Reversed control'],c.assets.map((name,i)=>[esc(name),exact(control.forward[i]),exact(control.reverse_counterfactual[i])]))+
    details('Declared transfer maps',mat(control.first_action,c.assets,c.assets)+mat(control.second_action,c.assets,c.assets)):`<p class="empty">Advance to publication 3 to compare both available contact maps. The earlier prefix cannot use the later repair.</p>`)+
  `<h2>Periodic phase without evaluating π</h2><p>A separate exact periodic carrier uses K = ℚ[z]/(z² − z + 1), with involution z ↦ 1 − z. Multiplication by z closes after six steps. The computation retains algebraic coordinates and its trace pairing; it does not estimate an angle or a circumference.</p>`+
  table(['Step','Constant coordinate','z coordinate'],reference('phase').coordinates.map((r,i)=>[String(i),exact(r[0]),exact(r[1])]))+
  `<p>This reference is available independently of the incident history. It demonstrates an exact declared phase carrier, not a claim that all rotations, periods or irrational numbers are represented by this one field.</p>`;};

const tensorsView=()=>{const d=row('database'),fib=row('fibers'),p=row('tensor_product')??row('tensor_product',0);return `<h1>Tensors and stalks</h1><p>Every event payload declares a rational tensor with shape 2 × 3. Position 0 holds forcing, position 1 is supported zero, and position 5 holds the optional reading when present. The other slots are absent. A tensor window keeps this distinction.</p>`+
  table(['Event','Declared shape','Supported slots','Nonzero values','Reading slot'],d.events.map(e=>[esc(e.event),'2 × 3',String(e.tensor_support),String(e.tensor_nonzeros),e.reading===null?'Absent':`Supported ${exact(e.reading)}`]))+
  `<h2>Local fibers, one global boundary action</h2><p>Each chain cell owns its declared stalk. Ordinary spans retain the two coordinate field (forcing, duration). The constant fiber realization explicitly maps both coordinates through each signed incidence. It validates the complete stalk family and applicable chain interfaces before execution.</p>`+
  mat(Array.from({length:fib.boundary_values.length/2},(_,i)=>fib.boundary_values.slice(2*i,2*i+2)),row('field').time_coordinates.map(t=>`Minute ${t}`),fib.fiber_coordinates)+
  `<p>The native field cache records a warm result hit for this exact source at publication ${fib.source_sequence}. Across the run it has prepared ${fib.compiled_plans} distinct plans and applied ${fib.applications} uncached field actions. Equal output values alone do not authorize cache reuse.</p>`+
  `<h2>Time span × asset interface</h2><p>The native tensor product has separate time and asset owners. Its boundary is the graded Leibniz boundary, including the sign determined by the left grade. A second boundary evaluates to exact supported zero.</p>`+
  eq('∂(a ⊗ b) = ∂a ⊗ b + (−1)<sup>grade(a)</sup> a ⊗ ∂b<br>∂²(a ⊗ b) = 0')+
  `<p>The displayed product receipt uses time publication ${p.sequence} and the independently declared asset interface at publication ${p.asset_cutoff}. It retains ${p.retained_occurrences} boundary occurrences and ${p.supported_second_boundary} supported coordinates in the second boundary.</p>`+
  table(['Time grade','Time coordinate','Asset grade','Asset coordinate','Boundary value'],p.boundary.map(e=>[String(e.time_grade),String(e.time_index),String(e.asset_grade),String(e.asset_index),exact(e.value)]))+
  `<p class="caption">The product is executed for the initial and final time endpoints. Intermediate views show the explicitly named initial product receipt, rather than pretending it was recomputed for another owner.</p>`+
  `<h2>Complete owning map continuation</h2><p>A separate canonical three factor reference composes three declared chain maps using twelve original endpoint roles. Both parenthesizations agree through their declared associator. Continuation retains the actual prior lift and map receipt, rather than identifying them through equal output bytes.</p>`;};

const comparisonsView=()=>{const pr=reference('pagerank'),text=reference('textrank'),unit=records.find(r=>r.chapter==='variation'&&r.sequence===cutoff&&r.metric==='unit'),weighted=records.find(r=>r.chapter==='variation'&&r.sequence===cutoff&&r.metric==='duration');return `<h1>Exact comparison cases</h1><p>These comparisons state their hypotheses. They identify exact algebraic relationships; they do not claim that every statistical model is equivalent to the relational field calculus.</p>`+
  `<h2>PageRank as a declared reversible field response</h2><p>For a reversible weighted pair realization with conductances 1 and 2, degree metric M₀ = D⁻¹ and relation metric M₁ = W⁻¹ give L₀ = I − T. With α = 1/2 and λ = α/(1 − α) = 1, the PageRank vector and the Green response are exactly equal.</p>`+
  eq('(I − αT)r = (1 − α)v<br>(I + λL₀)r = v')+
  table(['Coordinate','PageRank','Green response'],pr.rank.map((q,i)=>[String(i),exact(q),exact(pr.green_response[i])]))+
  details('Reversible reference operators',mat(pr.transition)+mat(pr.hodge_operator))+
  `<p class="caption">The identity depends on the declared reversible realization and metric. It is not asserted for every directed transition operator.</p>`+
  `<h2>TextRank normalization from retained participation</h2><p>Three primary sentence relations have lengths 2, 4 and 8, one common term and disjoint private terms. Participation X is a derived unsigned view of their retained occurrences. Signed chain boundary and unsigned participation remain separate.</p>`+
  eq('A = XᵀX − diag(XᵀX)<br>R<sub>ij</sub> = A<sub>ij</sub> / (m<sub>i</sub> + m<sub>j</sub>), &nbsp; |S<sub>i</sub>| = 2<sup>m<sub>i</sub></sup>')+
  `<p>The common log(2) factor cancels under column normalization. This gives an exact rational instance of the published logarithmic weighting, using the declared common base subclass.</p>`+
  table(['Sentence length','Exact normalized rank','Raw overlap control'],text.lengths.map((length,i)=>[String(length),exact(text.rank[i]),exact(text.raw_overlap_rank[i])]))+
  `<p class="caption">General unequal sentence lengths can require nonrational logarithmic weights. The demonstration does not substitute a rational approximation for that case.</p>`+
  `<h2>Exact typed variation and the ANOVA decomposition</h2><p>For the observed forcing values, declared category frames define global and group mean projections. The unit metric gives the familiar deterministic between and within sum of squares identity. The duration metric gives a different, explicitly declared weighted identity.</p>`+
  eq('f − global mean = between groups + within groups<br>⟨between, within⟩<sub>M</sub> = 0<br>Total quadrance = between quadrance + within quadrance')+
  table(['Declared metric','Global mean','Between quadrance','Within quadrance','Total quadrance'],[unit,weighted].map(r=>[esc(r.metric),exact(r.global_mean),exact(r.between_quadrance),exact(r.within_quadrance),exact(r.total_quadrance)]))+
  `<p>This exact decomposition needs no distributional approximation. An F test, significance level or causal conclusion would need additional statistical assumptions and is not produced.</p>`;};

const reproduceView=()=>{const checked=DEMO.verification;return `<h1>Sources and reproduction</h1><p>The native demonstration produces portable typed publications, exact update frames and a durable field history. The HTML report is a presentation of those completed computations. The local query server invokes the C++ consumer against the selected native file.</p>`+
  `<h2>Run from the distributed library</h2>`+code('python3 rexgraphcpp/examples/temporal_dynamics/run.py --verify --package --serve')+
  `<p>The first run visibly configures and compiles the C++ consumer. Later runs reuse the build. Each run creates a new output directory. Existing demonstrations are preserved.</p>`+
  code('python3 rexgraphcpp/examples/temporal_dynamics/serve.py PATH_TO_DEMO --exe build/temporal-dynamics-demo/cpp/temporal_dynamics')+
  `<p>To use the full notebook application, add --notebook-exe build/notebook/rfcnb when generating the demonstration. Then serve the generated temporal-dynamics.rfnb with its separate notebook-state directory. Opening a notebook does not execute its cells.</p>`+
  `<h2>Fresh demonstration verification</h2>`+
  (checked?`<span class="badge passed">Independent exact checks passed</span><div class="cards">${card(checked.independent_exact_checks,'Independent exact assertions')}${card(checked.historical_prefixes,'Independently replayed prefixes')}${card(checked.future_suffix_controls,'Perturbed future suffix controls')}</div><p>Native assertion checks: ${checked.native_checks}. Each truncated history matches its corresponding results and canonical native files from the complete history. Altering all later forcing and reading values leaves the earlier observations unchanged.</p>`:`<p class="notice">Native assertions completed. Generate with --verify to also run the independent exact oracle, every truncated history and future suffix controls.</p>`)+
  `<p>Authority checks also reject a later file at an earlier cutoff, a foreign namespace, a malformed native file and an existing output directory. An invalid higher boundary rejects through the adjacent chain axiom. A retained old field plan still executes against its original publication after later durable appends.</p>`+
  `<h2>Native artifacts</h2>`+table(['File','Meaning'],[['database/publication-N.rfcxi','Exact interpreted database endpoint'],['database/transition-N.rfcxiu','Lossless typed assertion update'],['fields/transition-N.rfcxt','Exact field endpoint transition with declared carrier maps'],['fields/history.rfcxs','Portable native field history'],['field-store/','Exclusive local durable native history'],['temporal-dynamics.rfnb','Runnable native query walkthrough'],['observations.json','Exact derived observations and source witnesses'],['manifest.json','Artifact hashes and execution scope']])+`<p><a href="manifest.json">Open artifact manifest</a> <a href="observations.json">Open exact observations</a> <a href="fields/history.rfcxs" download>Download native field history</a></p>`+
  `<h2>Consolidated example catalog</h2><p>${DEMO.catalog.examples.length} standalone C++ examples are indexed below. The walkthrough combines their relevant interfaces into the one temporal incident. The catalog preserves each separate example and its build instructions; an entry alone is not a new execution result.</p>`+
  Object.keys(DEMO.catalog.examples.reduce((g,x)=>(g[x.group]=true,g),{})).map(g=>`<h3>${esc(g)}</h3>`+DEMO.catalog.examples.filter(x=>x.group===g).map(x=>`<details class="catalog"><summary>${esc(x.name)}</summary><div><p>${esc(x.readme)}</p>${code(x.build.join('\n'))}<p class="caption">${esc(x.qualification)}</p></div></details>`).join('')).join('')+
  `<h2>Existing notebook walkthroughs</h2><p>The original notebook files remain separate and unchanged. This generated notebook brings their temporal query workflow into one incident.</p>`+
  table(['Notebook','Source'],DEMO.catalog.notebooks.map(x=>[esc(x.name),esc(x.source)]))+
  `<h2>Reviewed research families</h2><p>The retained nineteen experiment families and three internal language studies inform the relevant chapters. Their exact, numerical and unresolved scopes remain distinct. In particular, a threshold filtration of an already observed corpus cannot serve as a demonstration of event availability without leakage.</p>`+
  table(['Experiment','Subject','Related chapter'],DEMO.catalog.research.map(x=>[esc(x.name),esc(x.subject),esc(x.chapter)]))+
  `<p>The original paper is the mathematical authority. Relevant definitions are retained occurrences and composite binary coefficients, declared chains, metric adjoints, temporal moments, ordered operation words and finite update channels. The companion's corrected product and temporal chapters provide the research context. No original experiment or paper is modified by this demonstration.</p>`;};

const views={introduction:intro,timeline:timelineView,queries:queryView,structure:structureView,fields:fieldView,changes:changesView,chronology:chronologyView,tensors:tensorsView,comparisons:comparisonsView,reproduce:reproduceView};
function draw(){
  queryEpoch++;
  $('view').innerHTML=views[chapter]();
  document.querySelectorAll('#chapters button').forEach(b=>b.setAttribute('aria-current',b.dataset.chapter===chapter?'page':'false'));
  $('previous').disabled=cutoff===0;$('next').disabled=cutoff===databases.at(-1).sequence;
  $('publication').value=String(cutoff);
  $('availability').textContent=`Available at minute ${row('database').available_minute}. Source publication ${cutoff}.`;
  $('backChapter').disabled=chapters.indexOf(chapter)===0;$('nextChapter').disabled=chapters.indexOf(chapter)===chapters.length-1;
  if(chapter==='queries')bindQuery();
}
function navigate(c){chapter=c;draw();$('content').scrollIntoView({block:'start'});}
async function bindQuery(){
  if(nativeToken){$('runQuery').disabled=false;$('queryStatus').textContent='Native query service ready. New queries run against the selected publication.';}
  $('queryForm').onsubmit=async event=>{
    event.preventDefault();const epoch=queryEpoch,selected=cutoff;
    const start=$('queryStart').value.trim(),end=$('queryEnd').value.trim();
    $('runQuery').disabled=true;$('queryStatus').textContent=`Running native query at publication ${selected}…`;
    try{
      const response=await fetch('/api/query',{method:'POST',headers:{'Content-Type':'application/json','X-RFC-Demo-Token':nativeToken},body:JSON.stringify({cutoff:selected,start,end})});
      const result=await response.json();if(!response.ok)throw Error(result.error);
      if(epoch!==queryEpoch)return;
      if(result.cutoff!==selected||result.source_sequence!==selected)throw Error('Native query returned a different endpoint');
      $('queryResult').innerHTML=`<h3>Completed native query for [${esc(start)}, ${esc(end)})</h3>`+queryResult(result);
      $('queryStatus').textContent=`Complete. Native publication ${selected}, exact retained output.`;
    }catch(error){if(epoch===queryEpoch)$('queryStatus').textContent=`Query failed: ${error.message}`;}
    finally{if(epoch===queryEpoch)$('runQuery').disabled=false;}
  };
}

databases.forEach(d=>{const o=document.createElement('option');o.value=String(d.sequence);o.textContent=String(d.sequence);$('publication').append(o);});
$('publication').onchange=()=>{cutoff=Number($('publication').value);draw();};
$('previous').onclick=()=>{if(cutoff>0){cutoff--;draw();}};
$('next').onclick=()=>{if(cutoff<databases.at(-1).sequence){cutoff++;draw();}};
document.querySelectorAll('#chapters button').forEach(b=>b.onclick=()=>navigate(b.dataset.chapter));
$('backChapter').onclick=()=>navigate(chapters[chapters.indexOf(chapter)-1]);
$('nextChapter').onclick=()=>navigate(chapters[chapters.indexOf(chapter)+1]);
function appearance(theme){document.documentElement.dataset.theme=theme;$('theme').textContent=theme==='dark'?'Light appearance':'Dark appearance';$('theme').setAttribute('aria-label',`Switch to ${theme==='dark'?'light':'dark'} appearance`);}
try{appearance(localStorage.getItem('rfc-temporal-demo-theme')||'light');}catch(_){appearance('light');}
$('theme').onclick=()=>{const theme=document.documentElement.dataset.theme==='dark'?'light':'dark';appearance(theme);try{localStorage.setItem('rfc-temporal-demo-theme',theme);}catch(_){}};
let printing=false;
$('print').onclick=()=>{printing=true;$('view').innerHTML=chapters.map(c=>`<section>${views[c]()}</section>`).join('');window.print();};
window.onafterprint=()=>{if(printing){printing=false;draw();}};
draw();
if(location.protocol==='http:'&&location.hostname==='127.0.0.1')fetch('/api/status').then(r=>r.json()).then(r=>{if(r.native_queries){nativeToken=r.token;if(chapter==='queries')bindQuery();}}).catch(()=>{});
