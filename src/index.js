const DATA_FILE = 'events.json';
const UNIT_BYTES = 128;
const GRID_COLS = 32;
const PAGE_BYTES = UNIT_BYTES * GRID_COLS;
const ANIMATION_MS = 430;

const state = {
    events: [],
    timeline: [],
    step: 0,
    isPlaying: false,
    playTimer: null,
    cleanupTimer: null,
    previousOccupancy: new Map(),
    pendingRemovedRegions: [],
    renderMode: 'steady'
};

const el = {
    resetBtn: document.getElementById('resetBtn'),
    prevBtn: document.getElementById('prevBtn'),
    playBtn: document.getElementById('playBtn'),
    nextBtn: document.getElementById('nextBtn'),
    stepSlider: document.getElementById('stepSlider'),
    stepLabel: document.getElementById('stepLabel'),
    eventLabel: document.getElementById('eventLabel'),
    memoryGrid: document.getElementById('memoryGrid'),
    summary: document.getElementById('summary'),
    regionLegend: document.getElementById('regionLegend')
};

function parseSize(value) {
    const n = Number(value ?? 0);
    return Number.isFinite(n) ? n : 0;
}

function formatBytes(bytes) {
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(bytes % 1024 === 0 ? 0 : 1)} KiB`;
    return `${(bytes / (1024 * 1024)).toFixed(2)} MiB`;
}

function describeEvent(event) {
    if (!event) return 'Initial state';
    const allocatorId = event.allocator_id ?? 'allocator';

    if (event.kind === 'allocation') {
        return `Allocation ${allocatorId} @ ${event.allocated_ptr} (${formatBytes(parseSize(event.allocated_size))})`;
    }
    if (event.kind === 'reallocation') {
        return `Reallocation ${allocatorId} ${event.old_ptr} -> ${event.allocated_ptr} (${formatBytes(parseSize(event.old_size))} -> ${formatBytes(parseSize(event.allocated_size))})`;
    }
    if (event.kind === 'deallocation') {
        return `Deallocation ${allocatorId} @ ${event.deallocated_ptr} (${formatBytes(parseSize(event.deallocated_size))})`;
    }
    return event.kind || 'Unknown';
}

function hashString(input) {
    let hash = 2166136261;
    for (let i = 0; i < input.length; i++) {
        hash ^= input.charCodeAt(i);
        hash = Math.imul(hash, 16777619);
    }
    return hash >>> 0;
}

function colorForAllocator(allocatorId) {
    const hash = hashString(String(allocatorId ?? 'unknown_allocator'));
    const hue = hash % 360;
    return `hsl(${hue} 65% 52%)`;
}

function makeLiveKey(allocatorId, ptr) {
    return `${allocatorId}:${ptr}`;
}

function makeSegmentKey(pageBase, allocatorId, startOffset, endOffset) {
    return `${pageBase}:${allocatorId}:${startOffset}:${endOffset}`;
}

function liveEntries(memoryMap) {
    return Array.from(memoryMap.entries());
}

function isWithinRange(base, start, size) {
    return base >= start && base < start + size;
}

function regionContains(outerBase, outerSize, innerBase, innerSize) {
    return innerBase >= outerBase && (innerBase + innerSize) <= (outerBase + outerSize);
}

function inferBackingParent(region, allRegions) {
    let bestParent = null;
    for (const candidate of allRegions) {
        if (candidate.live_key === region.live_key) continue;
        if (candidate.allocator_id === region.allocator_id) continue;
        if (!regionContains(candidate.base, candidate.size, region.base, region.size)) continue;
        if (!bestParent || candidate.size < bestParent.size) {
            bestParent = candidate;
        }
    }
    return bestParent;
}

function annotateInferredParents(memoryMap) {
    const allRegions = Array.from(memoryMap.values()).filter(region => Number.isFinite(region.base));
    const annotated = new Map();

    for (const [key, region] of memoryMap.entries()) {
        const parent = inferBackingParent(region, allRegions);
        annotated.set(key, {
            ...region,
            parent_allocator_id: parent ? parent.allocator_id : null,
            inferred_parent_live_key: parent ? parent.live_key : null,
            inferred_parent_ptr: parent ? parent.ptr : null
        });
    }

    return annotated;
}

function findLiveRegionForDeallocation(next, allocatorId, ptr) {
    const liveKey = makeLiveKey(allocatorId, ptr);
    if (next.has(liveKey)) {
        return [liveKey, next.get(liveKey)];
    }

    if (ptr === '0x0' || ptr === '0x00000000' || ptr === '0') {
        let lastMatch = null;
        for (const [key, region] of liveEntries(next)) {
            if (region.allocator_id !== allocatorId) continue;
            lastMatch = [key, region];
        }
        return lastMatch ?? [null, null];
    }

    for (const [key, region] of liveEntries(next)) {
        if (region.allocator_id !== allocatorId) continue;
        if (region.ptr === ptr) return [key, region];
        if (region.original_ptr === ptr) return [key, region];
        if (Array.isArray(region.known_ptrs) && region.known_ptrs.includes(ptr)) return [key, region];
    }

    const pointerMatches = [];
    for (const [key, region] of liveEntries(next)) {
        if (region.ptr === ptr || region.original_ptr === ptr || (Array.isArray(region.known_ptrs) && region.known_ptrs.includes(ptr))) {
            pointerMatches.push([key, region]);
        }
    }

    if (pointerMatches.length === 1) {
        return pointerMatches[0];
    }

    return [null, null];
}

function deleteDescendants(next, annotatedBeforeDelete, parentRegion) {
    const toDelete = [];
    const removedStart = parentRegion.base;
    const removedEnd = parentRegion.base + parentRegion.size;

    for (const [key, region] of annotatedBeforeDelete.entries()) {
        if (region.inferred_parent_live_key !== parentRegion.live_key) continue;
        if (!Number.isFinite(region.base)) continue;
        if (region.base < removedStart) continue;
        if (region.base >= removedEnd) continue;
        toDelete.push(key);
    }

    for (const key of toDelete) {
        next.delete(key);
    }
}

function applyEvent(memoryMap, event) {
    const next = new Map(memoryMap);
    const allocatorId = event.allocator_id ?? 'allocator';

    if (event.kind === 'allocation') {
        const ptr = event.allocated_ptr;
        const liveKey = makeLiveKey(allocatorId, ptr);

        next.set(liveKey, {
            live_key: liveKey,
            ptr,
            original_ptr: ptr,
            known_ptrs: [ptr],
            allocator_id: allocatorId,
            parent_allocator_id: null,
            base: Number(ptr),
            size: parseSize(event.allocated_size)
        });
        return next;
    }

    if (event.kind === 'reallocation') {
        const oldPtr = event.old_ptr;
        const newPtr = event.allocated_ptr;
        const oldBase = Number(oldPtr);
        const newBase = Number(newPtr);
        const oldSize = parseSize(event.old_size);
        const newSize = parseSize(event.allocated_size);
        const delta = newBase - oldBase;
        const oldLiveKey = makeLiveKey(allocatorId, oldPtr);
        const existing = next.get(oldLiveKey);

        const annotatedBeforeMove = annotateInferredParents(next);
        const descendantsToMove = [];
        for (const [key, region] of annotatedBeforeMove.entries()) {
            if (region.inferred_parent_live_key !== oldLiveKey) continue;
            if (!Number.isFinite(region.base)) continue;
            if (!isWithinRange(region.base, oldBase, oldSize)) continue;
            descendantsToMove.push([key, region]);
        }

        next.delete(oldLiveKey);
        for (const [key] of descendantsToMove) {
            next.delete(key);
        }

        const newLiveKey = makeLiveKey(allocatorId, newPtr);
        next.set(newLiveKey, {
            live_key: newLiveKey,
            ptr: newPtr,
            original_ptr: existing ? existing.original_ptr : newPtr,
            known_ptrs: Array.from(new Set([...(existing?.known_ptrs ?? [oldPtr]), oldPtr, newPtr])),
            allocator_id: allocatorId,
            parent_allocator_id: null,
            base: newBase,
            size: newSize
        });

        for (const [, region] of descendantsToMove) {
            const movedBase = region.base + delta;
            const movedPtr = '0x' + movedBase.toString(16);
            const movedKey = makeLiveKey(region.allocator_id, movedPtr);
            next.set(movedKey, {
                ...region,
                live_key: movedKey,
                ptr: movedPtr,
                known_ptrs: Array.from(new Set([...(region.known_ptrs ?? [region.ptr]), region.ptr, movedPtr])),
                base: movedBase
            });
        }

        return next;
    }

    if (event.kind === 'deallocation') {
        const annotatedBeforeDelete = annotateInferredParents(next);
        const [liveKey, removedRegion] = findLiveRegionForDeallocation(next, allocatorId, event.deallocated_ptr);
        if (liveKey && removedRegion) {
            next.delete(liveKey);
            deleteDescendants(next, annotatedBeforeDelete, removedRegion);
        }
        return next;
    }

    return next;
}

function computeTimeline(events) {
    const timeline = [];
    let memoryMap = new Map();

    timeline.push({ step: 0, event: null, memoryMap: new Map() });

    for (let i = 0; i < events.length; i++) {
        const event = events[i];
        if (event.kind === 'allocation' || event.kind === 'reallocation' || event.kind === 'deallocation') {
            memoryMap = applyEvent(memoryMap, event);
        }
        timeline.push({ step: i + 1, event, memoryMap: new Map(memoryMap) });
    }

    return timeline;
}

function computeAnnotatedRegions(snapshot) {
    const annotatedMap = annotateInferredParents(snapshot.memoryMap);
    return Array.from(annotatedMap.values())
        .filter(region => Number.isFinite(region.base))
        .sort((a, b) => a.base - b.base || a.allocator_id.localeCompare(b.allocator_id))
        .map(region => ({
            ...region,
            allocator_color: colorForAllocator(region.allocator_id)
        }));
}

function spanForPage(base, size, pageBase) {
    const start = Math.max(base, pageBase);
    const end = Math.min(base + size, pageBase + PAGE_BYTES);
    if (end <= start) return null;

    return {
        startOffset: start - pageBase,
        endOffset: end - pageBase,
        widthBytes: end - start
    };
}

function allocatorUsageForInterval(regions, start, end) {
    const covering = regions.filter(region => region.base < end && (region.base + region.size) > start);
    const allocatorMap = new Map();

    for (const region of covering) {
        if (!allocatorMap.has(region.allocator_id)) {
            allocatorMap.set(region.allocator_id, {
                allocator_id: region.allocator_id,
                color: region.allocator_color,
                parent_allocator_id: region.parent_allocator_id,
                inferred_parent_live_key: region.inferred_parent_live_key,
                depth: 0
            });
        }
    }

    const allocatorIds = new Set(allocatorMap.keys());
    for (const entry of allocatorMap.values()) {
        let depth = 0;
        let currentParent = entry.parent_allocator_id;
        const seen = new Set();

        while (currentParent && allocatorIds.has(currentParent) && !seen.has(currentParent)) {
            seen.add(currentParent);
            depth += 1;
            const parentEntry = allocatorMap.get(currentParent);
            currentParent = parentEntry ? parentEntry.parent_allocator_id : null;
        }

        entry.depth = depth;
    }

    return Array.from(allocatorMap.values()).sort((a, b) => {
        if (a.depth !== b.depth) return a.depth - b.depth;
        if (a.parent_allocator_id === b.allocator_id) return -1;
        if (b.parent_allocator_id === a.allocator_id) return 1;
        return a.allocator_id.localeCompare(b.allocator_id);
    });
}

function buildIntervalMap(regions) {
    const pageToRegions = new Map();

    for (const region of regions) {
        const firstPageBase = Math.floor(region.base / PAGE_BYTES) * PAGE_BYTES;
        const lastPageBase = Math.floor((region.base + region.size - 1) / PAGE_BYTES) * PAGE_BYTES;

        for (let pageBase = firstPageBase; pageBase <= lastPageBase; pageBase += PAGE_BYTES) {
            if (!pageToRegions.has(pageBase)) {
                pageToRegions.set(pageBase, []);
            }
            pageToRegions.get(pageBase).push(region);
        }
    }

    const pageMap = new Map();
    for (const [pageBase, pageRegions] of pageToRegions.entries()) {
        const boundaries = new Set([0, PAGE_BYTES]);

        for (const region of pageRegions) {
            const span = spanForPage(region.base, region.size, pageBase);
            if (!span) continue;
            boundaries.add(span.startOffset);
            boundaries.add(span.endOffset);
        }

        const sorted = Array.from(boundaries).sort((a, b) => a - b);
        const intervals = [];

        for (let i = 0; i < sorted.length - 1; i++) {
            const startOffset = sorted[i];
            const endOffset = sorted[i + 1];
            if (endOffset <= startOffset) continue;

            const start = pageBase + startOffset;
            const end = pageBase + endOffset;
            const allocatorUsers = allocatorUsageForInterval(pageRegions, start, end);
            if (allocatorUsers.length === 0) continue;

            intervals.push({
                pageBase,
                startOffset,
                endOffset,
                widthBytes: endOffset - startOffset,
                allocatorUsers
            });
        }

        pageMap.set(pageBase, intervals);
    }

    return pageMap;
}

function buildFlatOccupancy(pageMap) {
    const flat = new Map();

    for (const [pageBase, intervals] of pageMap.entries()) {
        for (const interval of intervals) {
            for (const user of interval.allocatorUsers) {
                const key = makeSegmentKey(pageBase, user.allocator_id, interval.startOffset, interval.endOffset);
                flat.set(key, {
                    key,
                    pageBase,
                    allocator_id: user.allocator_id,
                    color: user.color,
                    parent_allocator_id: user.parent_allocator_id,
                    startOffset: interval.startOffset,
                    endOffset: interval.endOffset,
                    widthBytes: interval.widthBytes,
                    userCount: interval.allocatorUsers.length,
                    depth: user.depth
                });
            }
        }
    }

    return flat;
}

function computeAnimationPlan(prevFlat, nextFlat) {
    const added = new Set();
    const resizeHighlight = new Set();
    const resizeAdded = new Set();
    const removed = [];

    for (const [key, nextItem] of nextFlat.entries()) {
        const prevItem = prevFlat.get(key);
        if (!prevItem) {
            added.add(key);
            continue;
        }

        if (
            prevItem.widthBytes !== nextItem.widthBytes ||
            prevItem.startOffset !== nextItem.startOffset ||
            prevItem.endOffset !== nextItem.endOffset ||
            prevItem.userCount !== nextItem.userCount ||
            prevItem.depth !== nextItem.depth
        ) {
            resizeHighlight.add(key);
            if (
                nextItem.widthBytes > prevItem.widthBytes ||
                nextItem.startOffset !== prevItem.startOffset ||
                nextItem.endOffset !== prevItem.endOffset
            ) {
                resizeAdded.add(key);
            }
        }
    }

    for (const [key, prevItem] of prevFlat.entries()) {
        if (!nextFlat.has(key)) {
            removed.push(prevItem);
        }
    }

    return { added, resizeHighlight, resizeAdded, removed };
}

function buildLegend(regions) {
    const seen = new Set();
    const items = [];

    for (const region of regions) {
        const key = `${region.allocator_id}:${region.ptr}`;
        if (seen.has(key)) continue;
        seen.add(key);
        items.push(region);
    }

    el.regionLegend.innerHTML = items.map(item => `
        <div class="region-chip" title="${item.allocator_id} · ${item.ptr} · ${formatBytes(item.size)} · base ${'0x' + item.base.toString(16)}">
            <span class="region-chip-swatch" style="background:${item.allocator_color}"></span>
            <span class="region-chip-label">${item.allocator_id} @ ${item.ptr}</span>
        </div>
    `).join('');
}

function buildPageBases(pageMap, removedRegions) {
    const bases = new Set(pageMap.keys());
    for (const region of removedRegions) {
        bases.add(region.pageBase);
    }
    return Array.from(bases).sort((a, b) => a - b);
}

function verticalStyleForUser(index, count) {
    if (count <= 1) {
        return { top: '0%', height: '100%' };
    }
    const slice = 100 / count;
    return { top: `${index * slice}%`, height: `${slice}%` };
}

function getLogicalAffectedAllocatorIds(prevSnapshot, event) {
    const affected = new Set();
    if (!event) return affected;

    const allocatorId = event.allocator_id ?? null;
    if (!allocatorId) return affected;

    if (event.kind === 'allocation') {
        affected.add(allocatorId);
        return affected;
    }

    if (event.kind === 'reallocation') {
        affected.add(allocatorId);

        const oldPtr = event.old_ptr;
        const oldBase = Number(oldPtr);
        const oldSize = parseSize(event.old_size);
        const annotatedBeforeMove = annotateInferredParents(prevSnapshot.memoryMap);

        const parentLiveKey = makeLiveKey(allocatorId, oldPtr);
        for (const region of annotatedBeforeMove.values()) {
            if (region.inferred_parent_live_key !== parentLiveKey) continue;
            if (!Number.isFinite(region.base)) continue;
            if (!isWithinRange(region.base, oldBase, oldSize)) continue;
            affected.add(region.allocator_id);
        }
        return affected;
    }

    if (event.kind === 'deallocation') {
        const annotatedBeforeDelete = annotateInferredParents(prevSnapshot.memoryMap);
        const [, removedRegion] = findLiveRegionForDeallocation(prevSnapshot.memoryMap, allocatorId, event.deallocated_ptr);

        if (removedRegion) {
            affected.add(removedRegion.allocator_id);
            const removedStart = removedRegion.base;
            const removedEnd = removedRegion.base + removedRegion.size;

            for (const region of annotatedBeforeDelete.values()) {
                if (region.inferred_parent_live_key !== removedRegion.live_key) continue;
                if (!Number.isFinite(region.base)) continue;
                if (region.base < removedStart) continue;
                if (region.base >= removedEnd) continue;
                affected.add(region.allocator_id);
            }
        } else {
            affected.add(allocatorId);
        }

        return affected;
    }

    return affected;
}

function stopPlayback() {
    state.isPlaying = false;
    if (state.playTimer) {
        clearTimeout(state.playTimer);
        state.playTimer = null;
    }
}

function clearCleanupTimer() {
    if (state.cleanupTimer) {
        clearTimeout(state.cleanupTimer);
        state.cleanupTimer = null;
    }
}

function schedulePlayback() {
    if (!state.isPlaying) return;
    if (state.step >= state.timeline.length - 1) {
        stopPlayback();
        render();
        return;
    }

    state.playTimer = setTimeout(() => {
        state.step = Math.min(state.step + 1, state.timeline.length - 1);
        renderTransition();
        schedulePlayback();
    }, 900);
}

function renderGrid(snapshot, options = {}) {
    const animate = !!options.animate;
    const includeRemoved = !!options.includeRemoved;
    const prevSnapshot = options.prevSnapshot ?? null;
    const event = options.event ?? null;

    const regions = computeAnnotatedRegions(snapshot);
    const pageMap = buildIntervalMap(regions);
    const nextFlat = buildFlatOccupancy(pageMap);
    const prevFlat = state.previousOccupancy;

    const affectedAllocatorIds = (animate && prevSnapshot && event)
        ? getLogicalAffectedAllocatorIds(prevSnapshot, event)
        : new Set();

    const animationPlan = animate ? computeAnimationPlan(prevFlat, nextFlat) : {
        added: new Set(),
        resizeHighlight: new Set(),
        resizeAdded: new Set(),
        removed: []
    };

    if (animate) {
        state.pendingRemovedRegions = animationPlan.removed;
    }

    const removedRegions = includeRemoved ? state.pendingRemovedRegions : [];
    const pageBases = buildPageBases(pageMap, removedRegions);

    el.memoryGrid.innerHTML = '';
    buildLegend(regions);

    let shownPages = 0;

    for (const pageBase of pageBases) {
        const activeIntervals = pageMap.get(pageBase) || [];
        const renderables = [];

        for (const interval of activeIntervals) {
            const count = interval.allocatorUsers.length;
            interval.allocatorUsers.forEach((user, index) => {
                renderables.push({
                    key: makeSegmentKey(pageBase, user.allocator_id, interval.startOffset, interval.endOffset),
                    pageBase,
                    allocator_id: user.allocator_id,
                    color: user.color,
                    startOffset: interval.startOffset,
                    endOffset: interval.endOffset,
                    widthBytes: interval.widthBytes,
                    userCount: count,
                    verticalIndex: index,
                    depth: user.depth,
                    removed: false
                });
            });
        }

        for (const item of removedRegions.filter(item => item.pageBase === pageBase)) {
            renderables.push({
                ...item,
                verticalIndex: item.depth ?? 0,
                removed: true
            });
        }

        if (renderables.length === 0) continue;

        renderables.sort((a, b) =>
            a.startOffset - b.startOffset ||
            a.verticalIndex - b.verticalIndex ||
            a.allocator_id.localeCompare(b.allocator_id)
        );

        const rowEl = document.createElement('div');
        rowEl.className = 'memory-row';

        const addrEl = document.createElement('div');
        addrEl.className = 'row-address';
        addrEl.textContent = '0x' + pageBase.toString(16);
        rowEl.appendChild(addrEl);

        const barEl = document.createElement('div');
        barEl.className = 'page-bar';

        for (const item of renderables) {
            const regionEl = document.createElement('div');
            regionEl.className = 'page-region';
            regionEl.style.setProperty('--region-color', item.color);
            regionEl.style.left = `${(item.startOffset / PAGE_BYTES) * 100}%`;
            regionEl.style.width = `${Math.max((item.widthBytes / PAGE_BYTES) * 100, 0.15)}%`;

            const vertical = verticalStyleForUser(item.verticalIndex, item.userCount || 1);
            regionEl.style.top = vertical.top;
            regionEl.style.height = vertical.height;
            regionEl.title = `${item.allocator_id} · ${Math.max(item.widthBytes, 1)} B in page slice`;

            if (item.removed) {
                if (affectedAllocatorIds.has(item.allocator_id)) {
                    regionEl.classList.add('anim-removed');
                }
            } else if (animate && affectedAllocatorIds.has(item.allocator_id)) {
                if (animationPlan.resizeHighlight.has(item.key)) {
                    regionEl.classList.add('anim-resize-highlight');
                }
                if (animationPlan.resizeAdded.has(item.key)) {
                    regionEl.classList.add('anim-resize-added');
                } else if (animationPlan.added.has(item.key)) {
                    regionEl.classList.add('anim-added');
                }
            }

            barEl.appendChild(regionEl);
        }

        rowEl.appendChild(barEl);
        el.memoryGrid.appendChild(rowEl);
        shownPages += 1;
    }

    const totalBytes = regions.reduce((sum, region) => sum + region.size, 0);
    el.summary.textContent = `${formatBytes(totalBytes)} live · ${shownPages} pages shown`;
    state.previousOccupancy = nextFlat;
}

function render() {
    const maxStep = Math.max(0, state.timeline.length - 1);
    const safeStep = Math.min(state.step, maxStep);
    const snapshot = state.timeline[safeStep] || state.timeline[0];
    const prevSnapshot = safeStep > 0 ? state.timeline[safeStep - 1] : null;

    el.stepSlider.max = String(maxStep);
    el.stepSlider.value = String(safeStep);
    el.stepLabel.textContent = `Step ${safeStep} / ${maxStep}`;
    el.eventLabel.textContent = describeEvent(snapshot.event);
    el.prevBtn.disabled = safeStep === 0;
    el.nextBtn.disabled = safeStep >= maxStep;
    el.playBtn.disabled = maxStep === 0;
    el.playBtn.textContent = state.isPlaying ? 'Pause' : 'Play';

    renderGrid(snapshot, {
        animate: state.renderMode === 'transition',
        includeRemoved: state.renderMode === 'transition',
        prevSnapshot,
        event: snapshot.event
    });
}

function renderTransition() {
    clearCleanupTimer();
    state.renderMode = 'transition';
    render();
    state.cleanupTimer = setTimeout(() => {
        state.renderMode = 'steady';
        state.pendingRemovedRegions = [];
        render();
    }, ANIMATION_MS);
}

function bindEvents() {
    el.resetBtn.addEventListener('click', () => {
        stopPlayback();
        clearCleanupTimer();
        state.step = 0;
        state.previousOccupancy = new Map();
        state.pendingRemovedRegions = [];
        state.renderMode = 'steady';
        render();
    });

    el.prevBtn.addEventListener('click', () => {
        stopPlayback();
        clearCleanupTimer();
        state.step = Math.max(0, state.step - 1);
        renderTransition();
    });

    el.nextBtn.addEventListener('click', () => {
        stopPlayback();
        clearCleanupTimer();
        state.step = Math.min(state.timeline.length - 1, state.step + 1);
        renderTransition();
    });

    el.playBtn.addEventListener('click', () => {
        state.isPlaying = !state.isPlaying;
        if (state.isPlaying) {
            schedulePlayback();
        } else {
            stopPlayback();
        }
        render();
    });

    el.stepSlider.addEventListener('input', () => {
        stopPlayback();
        clearCleanupTimer();
        state.step = Number(el.stepSlider.value) || 0;
        renderTransition();
    });
}

async function loadEvents() {
    const response = await fetch(DATA_FILE, { cache: 'no-store' });
    if (!response.ok) {
        throw new Error(`Failed to load ${DATA_FILE}: ${response.status} ${response.statusText}`);
    }
    const data = await response.json();
    if (!Array.isArray(data)) {
        throw new Error(`${DATA_FILE} must contain a top-level JSON array.`);
    }
    return data;
}

async function init() {
    try {
        state.events = await loadEvents();
        state.timeline = computeTimeline(state.events);
        bindEvents();
        render();
    } catch (error) {
        document.querySelector('.app').innerHTML = `
            <h1>Allocation Event Grid Visualizer</h1>
            <section class="card">
                <div class="card-body">
                    <p class="error-text">${String(error.message)}</p>
                    <p style="margin-top:12px;">Create <code>${DATA_FILE}</code> next to <code>index.html</code> and put a JSON array of allocation events in it. Parent-child backing relationships are inferred from containment, so <code>parent_allocator_id</code> is optional.</p>
                </div>
            </section>
        `;
    }
}

init();