import { useEffect, useMemo, useRef, useState } from 'react';
import {
  COMPARISON_METRICS,
  WORKLOAD_GRAPH_GROUPS,
  formatMetricValue,
  getComparisonMetric,
  getConditionDifferences,
  getMetricDelta,
  toggleComparisonSelection,
} from './experimentDashboard.js';
import {
  MAX_WORKLOAD_IMPORT_BYTES,
  parseWorkloadText,
  WORKLOAD_CSV_TEMPLATE,
  workloadToCsv,
} from './workloadCsv.js';
import {
  createRandomWorkloadRequest,
  validateRandomWorkloadConfiguration,
} from './randomWorkload.js';

const SAMPLE_WORKLOAD = [
  { operation: 'allocate', size: 64 },
  { operation: 'allocate', size: 128 },
  { operation: 'allocate', size: 256 },
  { operation: 'free', allocation_id: 1 },
  { operation: 'allocate', size: 96 },
];

function formatNumber(value, digits = 2) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) {
    return '—';
  }
  const numeric = Number(value);
  return numeric >= 1000
    ? numeric.toLocaleString(undefined, { maximumFractionDigits: digits })
    : numeric.toFixed(digits);
}

function formatPercent(value, digits = 1) {
  if (value === null || value === undefined || Number.isNaN(Number(value))) {
    return '—';
  }
  return `${Number(value).toFixed(digits)}%`;
}

function toNumber(value, fallback = 0) {
  const numeric = Number(value);
  return Number.isFinite(numeric) ? numeric : fallback;
}

function getTone(successRate) {
  if (successRate >= 0.95) return 'stable';
  if (successRate >= 0.75) return 'watch';
  return 'critical';
}

function App() {
  const [poolData, setPoolData] = useState(null);
  const [poolCapacity, setPoolCapacity] = useState(262144);
  const [poolStrategy, setPoolStrategy] = useState('first_fit');
  const [fixedBlockSize, setFixedBlockSize] = useState(64);
  const [workloadText, setWorkloadText] = useState(JSON.stringify(SAMPLE_WORKLOAD, null, 2));
  const [allocationSize, setAllocationSize] = useState(64);
  const [selectedAllocationId, setSelectedAllocationId] = useState(null);
  const [randomOperationCount, setRandomOperationCount] = useState(40);
  const [randomMinimumSize, setRandomMinimumSize] = useState(16);
  const [randomMaximumSize, setRandomMaximumSize] = useState(256);
  const [randomAllocationProbability, setRandomAllocationProbability] = useState(70);
  const [randomDeallocationProbability, setRandomDeallocationProbability] = useState(30);
  const [randomSeed, setRandomSeed] = useState(42);
  const [randomStrategy, setRandomStrategy] = useState('first_fit');
  const [randomAllocationMode, setRandomAllocationMode] = useState('variable_size');
  const [randomFixedBlockSize, setRandomFixedBlockSize] = useState(256);
  const [randomExecutionConfiguration, setRandomExecutionConfiguration] = useState(null);
  const [statusMessage, setStatusMessage] = useState('Connecting to allocator…');
  const [poolError, setPoolError] = useState('');
  const [loadingPool, setLoadingPool] = useState(true);
  const [loading, setLoading] = useState(false);
  const [loadingExperiments, setLoadingExperiments] = useState(false);
  const [clearingExperiments, setClearingExperiments] = useState(false);
  const [loadingDetail, setLoadingDetail] = useState(false);
  const [loadingComparison, setLoadingComparison] = useState(false);
  const [workloadResults, setWorkloadResults] = useState([]);
  const [workloadSession, setWorkloadSession] = useState(null);
  const [benchmarkResults, setBenchmarkResults] = useState([]);
  const [experiments, setExperiments] = useState([]);
  const [selectedExperimentId, setSelectedExperimentId] = useState('');
  const [experimentExportFormat, setExperimentExportFormat] = useState('full');
  const [experimentDetail, setExperimentDetail] = useState(null);
  const [compareIds, setCompareIds] = useState([]);
  const [comparisonResults, setComparisonResults] = useState([]);
  const [historyError, setHistoryError] = useState('');
  const [detailError, setDetailError] = useState('');
  const [comparisonError, setComparisonError] = useState('');
  const [comparisonConditionsMatch, setComparisonConditionsMatch] = useState(null);
  const [comparisonConditionDifferences, setComparisonConditionDifferences] = useState([]);
  const [comparisonCsv, setComparisonCsv] = useState('');
  const [graphType, setGraphType] = useState('bar');
  const [graphMetric, setGraphMetric] = useState('average_allocation_latency_ms');
  const [showGraphLegend, setShowGraphLegend] = useState(true);
  const [showGraphValues, setShowGraphValues] = useState(true);
  const configurationInitialized = useRef(false);
  const randomWorkloadConfiguration = {
    operation_count: randomOperationCount,
    minimum_size: randomMinimumSize,
    maximum_size: randomMaximumSize,
    allocation_probability: randomAllocationProbability,
    deallocation_probability: randomDeallocationProbability,
    random_seed: randomSeed,
    strategy: randomStrategy,
    allocation_mode: randomAllocationMode,
    fixed_block_size: randomFixedBlockSize,
  };
  const randomWorkloadError = validateRandomWorkloadConfiguration(
    randomWorkloadConfiguration,
    poolData?.capacity ?? poolCapacity,
  );

  const loadPool = async () => {
    setLoadingPool(true);
    setPoolError('');
    try {
      const response = await fetch('/api/pool/state');
      const data = await response.json();
      if (!response.ok || !data.success || !data.payload || !Array.isArray(data.payload.blocks)) {
        throw new Error(data.message || 'Invalid allocator state response.');
      }
      const nextPool = data.payload;
      setPoolData(nextPool);
      if (!poolData) setStatusMessage('Connected to allocator backend.');
      if (!configurationInitialized.current) {
        setPoolCapacity(nextPool.capacity);
        setPoolStrategy(nextPool.pool_strategy || 'first_fit');
        setFixedBlockSize(nextPool.block_size || 64);
        configurationInitialized.current = true;
      }
      const allocatedBlocks = (nextPool.blocks ?? []).filter((block) => block.allocated);
      if (allocatedBlocks.length > 0 && (!selectedAllocationId || !allocatedBlocks.some((block) => block.allocation_id === selectedAllocationId))) {
        setSelectedAllocationId(allocatedBlocks[0].allocation_id);
      } else if (allocatedBlocks.length === 0) {
        setSelectedAllocationId(null);
      }
    } catch {
      setPoolError('Unable to load allocator state. No memory layout is shown until the backend responds.');
      setStatusMessage('Unable to reach allocator backend.');
    } finally {
      setLoadingPool(false);
    }
  };

  const handleClearComparison = () => {
    setCompareIds([]);
    setComparisonResults([]);
    setComparisonCsv('');
    setComparisonConditionsMatch(null);
    setComparisonConditionDifferences([]);
    setComparisonError('');
  };

  const handleExportComparison = () => {
    if (!comparisonCsv) return;
    const url = URL.createObjectURL(new Blob([comparisonCsv], { type: 'text/csv;charset=utf-8' }));
    const link = document.createElement('a');
    link.href = url;
    link.download = 'experiment-comparison.csv';
    link.click();
    URL.revokeObjectURL(url);
    setStatusMessage('Experiment comparison CSV exported.');
  };

  const loadExperiments = async () => {
    setLoadingExperiments(true);
    setHistoryError('');
    try {
      const response = await fetch('/api/experiments');
      const result = await response.json();
      if (!response.ok || !result.success) {
        throw new Error(result.message || 'Unable to load experiment history.');
      }
      const items = Array.isArray(result?.payload?.experiments) ? result.payload.experiments : [];
      const sorted = [...items].sort((a, b) => new Date(b.timestamp || 0) - new Date(a.timestamp || 0));
      setExperiments(sorted);
      setSelectedExperimentId((current) => sorted.some((item) => item.experiment_id === current) ? current : (sorted[0]?.experiment_id ?? ''));
      setCompareIds((current) => current.filter((id) => sorted.some((item) => item.experiment_id === id)));
    } catch {
      setHistoryError('Unable to load experiment history.');
    } finally {
      setLoadingExperiments(false);
    }
  };

  const handleClearExperiments = async () => {
    if (experiments.length === 0 || clearingExperiments) return;
    if (!window.confirm(`Clear all ${experiments.length} saved experiments? This cannot be undone.`)) {
      return;
    }

    setClearingExperiments(true);
    setHistoryError('');
    try {
      const response = await fetch('/api/experiments', { method: 'DELETE' });
      const result = await response.json();
      if (!response.ok || !result.success) {
        throw new Error(result.message || 'Unable to clear experiment history.');
      }
      setExperiments([]);
      setSelectedExperimentId('');
      setExperimentDetail(null);
      setCompareIds([]);
      setComparisonResults([]);
      setComparisonCsv('');
      setComparisonConditionsMatch(null);
      setComparisonConditionDifferences([]);
      setStatusMessage(result.message || 'Experiment history cleared.');
    } catch (error) {
      setHistoryError(error.message || 'Unable to clear experiment history.');
    } finally {
      setClearingExperiments(false);
    }
  };

  useEffect(() => {
    loadPool();
    loadExperiments();
  }, []);

  useEffect(() => {
    let active = true;
    if (!selectedExperimentId) {
      setExperimentDetail(null);
      setDetailError('');
      return undefined;
    }

    setLoadingDetail(true);
    setDetailError('');
    fetch(`/api/experiments/detail?id=${encodeURIComponent(selectedExperimentId)}`)
      .then(async (response) => {
        const result = await response.json();
        if (!response.ok || !result.success) {
          throw new Error(result.message || 'Unable to load experiment details.');
        }
        if (active) setExperimentDetail(result?.payload?.experiment ?? result?.payload ?? null);
      })
      .catch(() => {
        if (active) setDetailError('Unable to load experiment details.');
      })
      .finally(() => {
        if (active) setLoadingDetail(false);
      });

    return () => {
      active = false;
    };
  }, [selectedExperimentId]);

  const runAction = async (path, body) => {
    setLoading(true);
    try {
      const response = await fetch(path, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body ?? {}),
      });
      const result = await response.json();
      setStatusMessage(result.success ? (result.message || 'Operation succeeded') : (result.message || 'Operation failed'));
      await loadPool();
      return result;
    } catch {
      setStatusMessage('Backend request failed.');
      return null;
    } finally {
      setLoading(false);
    }
  };

  const handleAllocate = async () => {
    const result = await runAction('/api/allocate', { size: Number(allocationSize) || 64 });
    if (result?.success) {
      setStatusMessage(`${result.message} — allocation ID updated`);
    }
  };

  const handleFree = async () => {
    if (selectedAllocationId == null) {
      setStatusMessage('Select an allocated block to free it.');
      return;
    }
    await runAction('/api/free', { allocation_id: Number(selectedAllocationId) });
  };

  const handleReset = async () => {
    await runAction('/api/pool/reset');
    setSelectedAllocationId(null);
  };

  const handleInitializePool = async () => {
    if (poolData?.statistics?.active_allocations > 0
      && !window.confirm('Reinitializing the pool will discard its active allocations. Continue?')) {
      return;
    }
    const result = await runAction('/api/pool/initialize', {
      capacity: Number(poolCapacity),
      strategy: poolStrategy,
      fixed_block_size: Number(fixedBlockSize),
    });
    if (result?.success) {
      setStatusMessage('Pool initialized with the selected configuration.');
    }
  };

  const handleRunWorkload = async () => {
    let parsed;
    try {
      parsed = parseWorkloadText(workloadText);
    } catch (error) {
      setStatusMessage(error.message || 'Workload is invalid.');
      return;
    }

    setLoading(true);
    try {
      const response = await fetch('/api/workload/execute', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          strategy: poolData?.pool_strategy ?? 'first_fit',
          fixed_block_size: poolData?.block_size ?? 64,
          workload: parsed,
        }),
      });
      const result = await response.json();
      const payload = result?.payload ?? {};
      setWorkloadResults(Array.isArray(payload.results) ? payload.results : []);
      if (payload.state) setPoolData(payload.state);
      setStatusMessage(result.success ? (result.message || 'Workload executed') : (result.message || 'Workload failed'));
      if (result.success) {
        const active = (payload.state?.blocks ?? []).filter((block) => block.allocated);
        if (active[0]) setSelectedAllocationId(active[0].allocation_id);
        await loadExperiments();
      }
    } catch {
      setStatusMessage('Workload execution failed.');
    } finally {
      setLoading(false);
    }
  };

  const requestWorkloadSession = async (path, body = {}) => {
    try {
      const response = await fetch(path, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(body),
      });
      const result = await response.json();
      if (!response.ok || !result.success) {
        throw new Error(result.message || 'Workload session request failed.');
      }
      const session = result.payload ?? {};
      setWorkloadSession(session);
      if (Array.isArray(session.results)) setWorkloadResults(session.results);
      if (session.state) setPoolData(session.state);
      setStatusMessage(result.message || 'Workload session updated.');
      return session;
    } catch (error) {
      setStatusMessage(error.message || 'Workload session request failed.');
      return null;
    }
  };

  const handleStartWorkloadSession = async () => {
    let parsed;
    try {
      parsed = parseWorkloadText(workloadText);
    } catch (error) {
      setStatusMessage(error.message || 'Workload is invalid.');
      return;
    }
    await requestWorkloadSession('/api/workload/session/start', {
      strategy: poolData?.pool_strategy ?? 'first_fit',
      allocation_mode: poolData?.allocation_mode ?? 'variable_size',
      fixed_block_size: poolData?.block_size ?? 64,
      workload: parsed,
    });
  };

  const handleWorkloadStep = () => requestWorkloadSession('/api/workload/session/step');
  const handleWorkloadPause = () => requestWorkloadSession('/api/workload/session/pause');
  const handleWorkloadResume = () => requestWorkloadSession('/api/workload/session/resume');
  const handleWorkloadReset = () => {
    if (window.confirm('Reset the active workload session? Saved experiment history will not be deleted.')) {
      requestWorkloadSession('/api/workload/session/reset');
    }
  };

  const handleRunRandomWorkload = async () => {
    if (!poolData) return;
    if (randomWorkloadError) {
      setStatusMessage(randomWorkloadError);
      return;
    }
    setLoading(true);
    try {
      const response = await fetch('/api/workload/random', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify(createRandomWorkloadRequest(randomWorkloadConfiguration)),
      });
      const result = await response.json();
      if (!response.ok || !result.success) {
        throw new Error(result.message || 'Random workload failed.');
      }
      const payload = result.payload ?? {};
      setRandomExecutionConfiguration(payload.configuration ?? null);
      const operationResults = Array.isArray(payload.results) ? payload.results : [];
      setWorkloadResults(operationResults);
      if (payload.state) setPoolData(payload.state);
      setWorkloadText(JSON.stringify(operationResults.map((operation) => ({
        operation: operation.operation,
        ...(operation.operation === 'allocate' ? { size: operation.requested_size } : {}),
        allocation_id: operation.allocation_id,
      })), null, 2));
      setStatusMessage(result.message || 'Seeded random workload completed.');
      await loadExperiments();
    } catch (error) {
      setStatusMessage(error.message || 'Random workload request failed.');
    } finally {
      setLoading(false);
    }
  };

  const handleImportCsv = async (event) => {
    const file = event.target.files?.[0];
    if (!file) return;
    if (file.size > MAX_WORKLOAD_IMPORT_BYTES) {
      setStatusMessage(`CSV file exceeds the ${MAX_WORKLOAD_IMPORT_BYTES / 1024 / 1024} MiB import limit.`);
      event.target.value = '';
      return;
    }
    try {
      const parsed = parseWorkloadText(await file.text());
      setWorkloadText(JSON.stringify(parsed, null, 2));
      setStatusMessage(`Imported and validated ${parsed.length} workload entries.`);
    } catch (error) {
      setStatusMessage(error.message || 'CSV import failed validation.');
    } finally {
      event.target.value = '';
    }
  };

  const handleExportCsv = () => {
    let csv;
    try {
      csv = workloadToCsv(parseWorkloadText(workloadText));
    } catch (error) {
      setStatusMessage(error.message || 'Workload is invalid; CSV was not exported.');
      return;
    }
    const blob = new Blob([csv], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = 'allocator-workload.csv';
    link.click();
    URL.revokeObjectURL(url);
    setStatusMessage('Workload CSV exported.');
  };

  const handleDownloadCsvTemplate = () => {
    const blob = new Blob([WORKLOAD_CSV_TEMPLATE], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.download = 'allocator-workload-template.csv';
    link.click();
    URL.revokeObjectURL(url);
    setStatusMessage('Downloaded the workload CSV template.');
  };

  const handleBenchmark = async () => {
    if (!poolData) {
      setStatusMessage('Load allocator state before starting a benchmark.');
      return;
    }
    setLoading(true);
    try {
      const response = await fetch('/api/benchmark', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          capacity: poolData?.capacity ?? 0,
          fixed_block_size: poolData?.block_size ?? 64,
          strategies: ['first_fit', 'best_fit', 'fixed_size', 'variable_size'],
          sizes: [32, 64, 128, 256, 512],
          iterations: 10,
        }),
      });
      const result = await response.json();
      const rows = Array.isArray(result?.payload?.results) ? result.payload.results : [];
      setBenchmarkResults(rows);
      setStatusMessage(result.success ? 'Benchmark comparison completed.' : 'Benchmark failed.');
    } catch {
      setStatusMessage('Benchmark request failed.');
    } finally {
      setLoading(false);
    }
  };

  const handleExportExperiment = async () => {
    if (!experimentDetail?.experiment_id) return;
    try {
      const query = new URLSearchParams({
        id: experimentDetail.experiment_id,
        format: experimentExportFormat,
      });
      const response = await fetch(`/api/experiments/export?${query.toString()}`);
      const result = await response.json();
      if (!response.ok || !result.success || typeof result?.payload?.csv !== 'string') {
        throw new Error(result.message || 'Experiment export failed.');
      }
      const url = URL.createObjectURL(new Blob([result.payload.csv], { type: 'text/csv;charset=utf-8' }));
      const link = document.createElement('a');
      link.href = url;
      link.download = `${experimentDetail.experiment_id}-${experimentExportFormat}.csv`;
      link.click();
      URL.revokeObjectURL(url);
      setStatusMessage(`Experiment ${experimentExportFormat} CSV exported.`);
    } catch {
      setStatusMessage('Unable to export this experiment.');
    }
  };

  const requestComparison = async (ids) => {
    if (ids.length < 2) {
      setComparisonResults([]);
      setComparisonCsv('');
      setComparisonConditionsMatch(null);
      setComparisonConditionDifferences([]);
      return;
    }
    setLoadingComparison(true);
    setComparisonError('');
    try {
      const response = await fetch('/api/experiments/compare', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ experiment_ids: ids }),
      });
      const result = await response.json();
      if (!response.ok || !result.success) {
        throw new Error(result.message || 'Unable to compare experiments.');
      }
      const rows = Array.isArray(result?.payload?.comparison) ? result.payload.comparison : [];
      const selectedById = new Map(experiments.map((experiment) => [experiment.experiment_id, experiment]));
      setComparisonResults(rows.map((row) => {
        const saved = selectedById.get(row.experiment_id);
        return {
          ...row,
          pool_capacity: saved?.pool_capacity,
          allocated_memory: saved?.allocated_memory,
          free_memory: saved?.free_memory,
        };
      }));
      setComparisonCsv(typeof result?.payload?.csv === 'string' ? result.payload.csv : '');
      setComparisonConditionsMatch(result?.payload?.same_conditions ?? null);
      setComparisonConditionDifferences(Array.isArray(result?.payload?.condition_differences) ? result.payload.condition_differences : []);
      setStatusMessage(rows.length > 0 ? 'Experiment comparison refreshed.' : 'No comparable experiments available.');
    } catch (error) {
      setComparisonError(error.message || 'Unable to compare experiments.');
    } finally {
      setLoadingComparison(false);
    }
  };

  const handleToggleCompare = (experimentId) => {
    const nextIds = toggleComparisonSelection(compareIds, experimentId);
    setCompareIds(nextIds);
    setComparisonResults([]);
    setComparisonCsv('');
    setComparisonConditionsMatch(null);
    setComparisonConditionDifferences([]);
    if (nextIds.length >= 2) requestComparison(nextIds);
  };

  const handleCompareSelected = async () => {
    if (compareIds.length < 2) {
      setComparisonError('Select at least two experiments to compare.');
      return;
    }
    await requestComparison(compareIds);
  };

  const handleRefreshComparison = async () => {
    await loadExperiments();
    await requestComparison(compareIds);
  };

  const allocatedBlocks = (poolData?.blocks ?? []).filter((block) => block.allocated);
  const selectedExperiment = useMemo(
    () => experiments.find((experiment) => experiment.experiment_id === selectedExperimentId) ?? null,
    [experiments, selectedExperimentId],
  );

  const comparisonChartRows = useMemo(() => {
    if (!comparisonResults.length) return [];
    const maxExec = Math.max(...comparisonResults.map((row) => toNumber(row.execution_time_ms, 0)), 1);
    const maxLatency = Math.max(...comparisonResults.map((row) => toNumber(row.average_allocation_latency_ms, 0)), 1);
    return comparisonResults.map((row) => ({
      ...row,
      execWidth: Math.min(100, (toNumber(row.execution_time_ms, 0) / maxExec) * 100),
      latencyWidth: Math.min(100, (toNumber(row.average_allocation_latency_ms, 0) / maxLatency) * 100),
      successRate: toNumber(row.allocation_success_rate, 0) * 100,
    }));
  }, [comparisonResults]);

  const summaryExperiment = experimentDetail;
  const locallyDetectedConditionDifferences = getConditionDifferences(
    experiments.filter((experiment) => compareIds.includes(experiment.experiment_id)),
  );
  const selectedComparisonExperiments = experiments.filter((experiment) => compareIds.includes(experiment.experiment_id));
  const selectedGraphMetric = getComparisonMetric(graphMetric);
  const graphRows = useMemo(
    () => comparisonResults.map((row, index) => ({
      ...row,
      graphIndex: index,
      graphValue: row[graphMetric],
    })),
    [comparisonResults, graphMetric],
  );
  const renderWorkloadGraph = (group) => {
    const metrics = group.metrics
      .map((key) => getComparisonMetric(key) ?? (key === 'pool_capacity'
        ? { key, label: 'Pool capacity', unit: 'bytes' }
        : key === 'allocated_memory'
          ? { key, label: 'Allocated memory', unit: 'bytes' }
          : key === 'free_memory'
            ? { key, label: 'Free memory', unit: 'bytes' }
            : { key, label: key.replaceAll('_', ' '), unit: 'unavailable' }));
    const rows = comparisonResults;
    const values = rows.flatMap((row) => metrics.map((metric) => {
      const value = Number(row[metric.key]);
      return Number.isFinite(value) ? value * (metric.fraction ? 100 : 1) : null;
    }).filter((value) => value !== null));
    const maxValue = Math.max(...values, 1);
    return (
      <div className="analytics-chart" key={group.key}>
        <div className="analytics-chart-heading">
          <h4>{group.title}</h4>
          <span>{graphType === 'bar' ? 'Grouped bars' : 'Metric trend'}</span>
        </div>
        {graphType === 'bar' ? (
          <div className="grouped-bars" role="img" aria-label={`${group.title} comparison chart`}>
            {rows.map((row, rowIndex) => (
              <div className="chart-group" key={row.experiment_id || rowIndex}>
                <div className="chart-group-bars">
                  {metrics.map((metric) => {
                    const raw = Number(row[metric.key]);
                    const value = Number.isFinite(raw) ? raw * (metric.fraction ? 100 : 1) : null;
                    return (
                      <div className="chart-bar-column" key={metric.key} title={`${row.name || row.strategy}: ${metric.label} — ${value === null ? 'Unavailable' : formatMetricValue(metric, raw)}`}>
                        <div className="chart-bar-value">{showGraphValues && value !== null ? formatMetricValue(metric, raw) : ''}</div>
                        <div className="analytics-bar" style={{ height: `${value === null ? 0 : Math.max(4, (value / maxValue) * 100)}%`, background: `hsl(${160 + ((rowIndex * 35) % 140)} 55% 42%)` }} />
                      </div>
                    );
                  })}
                </div>
                <strong>{row.name || row.strategy}</strong>
              </div>
            ))}
          </div>
        ) : (
          <div className="line-chart-wrap">
            <svg viewBox="0 0 600 180" role="img" aria-label={`${group.title} trend chart`}>
              {metrics.map((metric, metricIndex) => {
                const points = rows.map((row, index) => {
                  const raw = Number(row[metric.key]);
                  const value = Number.isFinite(raw) ? raw * (metric.fraction ? 100 : 1) : null;
                  return `${rows.length === 1 ? 300 : 24 + (index * 552) / (rows.length - 1)},${value === null ? 170 : 170 - (value / maxValue) * 145}`;
                }).join(' ');
                return <polyline key={metric.key} points={points} fill="none" stroke={`hsl(${160 + metricIndex * 35} 55% 42%)`} strokeWidth="3" />;
              })}
            </svg>
            <div className="line-chart-labels">{rows.map((row) => <span key={row.experiment_id}>{row.name || row.strategy}</span>)}</div>
          </div>
        )}
        {showGraphLegend && <div className="chart-legend">{metrics.map((metric, index) => <span key={metric.key}><i style={{ background: `hsl(${160 + index * 35} 55% 42%)` }} />{metric.label}</span>)}</div>}
      </div>
    );
  };
  const renderMetric = (label, value, suffix = '') => (
    <div className="mini-card">
      <span>{label}</span>
      <strong>{value === null || value === undefined ? '—' : `${value}${suffix}`}</strong>
    </div>
  );

  return (
    <div className="app-shell">
      <header className="topbar">
        <div>
          <p className="eyebrow">Allocator overview</p>
          <h1>Memory Pool Allocator Dashboard</h1>
        </div>
        <div className="status-pill">{statusMessage}</div>
      </header>
      {poolError && <div className="state-box error" role="alert">{poolError}</div>}

      <section className="toolbar panel">
        <label>
          Pool capacity (bytes)
          <input type="number" min="1" max="67108864" value={poolCapacity} onChange={(event) => setPoolCapacity(event.target.value)} />
        </label>
        <label>
          Allocation strategy
          <select value={poolStrategy} onChange={(event) => setPoolStrategy(event.target.value)}>
            <option value="first_fit">First-Fit variable-size</option>
            <option value="best_fit">Best-Fit variable-size</option>
            <option value="variable_size">Variable-size (First-Fit)</option>
            <option value="fixed_size">Fixed-size</option>
          </select>
        </label>
        {poolStrategy === 'fixed_size' && (
          <label>
            Fixed block size (bytes)
            <input type="number" min="1" value={fixedBlockSize} onChange={(event) => setFixedBlockSize(event.target.value)} />
          </label>
        )}
        <button type="button" onClick={handleInitializePool} disabled={loading}>Initialize pool</button>
        <label>
          Allocation size
          <input type="number" min="8" step="8" value={allocationSize} onChange={(event) => setAllocationSize(event.target.value)} />
        </label>
        <button type="button" onClick={handleAllocate} disabled={loading || !poolData}>Allocate</button>
        <button type="button" onClick={handleFree} disabled={loading || !poolData || selectedAllocationId == null}>Free selected</button>
        <button type="button" onClick={handleReset} disabled={loading || !poolData}>Reset pool</button>
      </section>

      <section className="summary-grid">
        <div className="metric-card">
          <span className="metric-label">Active allocation strategy</span>
          <strong>{poolData?.pool_strategy?.replaceAll('_', ' ') ?? '—'}</strong>
        </div>
        <div className="metric-card">
          <span className="metric-label">Total capacity</span>
          <strong>{formatNumber(poolData?.capacity, 0)}</strong>
        </div>
        <div className="metric-card">
          <span className="metric-label">Free memory</span>
          <strong>{formatNumber(poolData?.statistics?.free_memory, 0)}</strong>
        </div>
        <div className="metric-card">
          <span className="metric-label">Allocated memory</span>
          <strong>{formatNumber(poolData?.statistics?.allocated_memory, 0)}</strong>
        </div>
        <div className="metric-card">
          <span className="metric-label">Largest free block</span>
          <strong>{formatNumber(poolData?.statistics?.largest_free_block, 0)}</strong>
        </div>
        <div className="metric-card">
          <span className="metric-label">Utilization</span>
          <strong>{poolData ? formatPercent((poolData.statistics?.memory_utilization ?? 0) * 100, 1) : '—'}</strong>
        </div>
      </section>

      <section className="memory-panel panel">
        <div className="section-head">
          <h3>Memory layout</h3>
        </div>
        <div className="memory-visualization">
          {!poolData ? (
            <div className={`state-box ${poolError ? 'error' : 'empty'}`} role={poolError ? 'alert' : undefined}>
              {loadingPool ? 'Loading memory state from the C++ backend…' : poolError}
            </div>
          ) : poolData.blocks.map((block) => (
            <button
              key={block.id}
              type="button"
              className={`memory-block ${block.allocated ? 'allocated' : 'free'} ${selectedAllocationId === block.allocation_id ? 'selected' : ''}`}
              style={{ width: `${(block.size / Math.max(poolData.capacity, 1)) * 100}%` }}
              title={`Block ${block.id} | Offset ${block.offset} | Size ${block.size} | Allocation ${block.allocation_id}`}
              onClick={() => block.allocated && setSelectedAllocationId(block.allocation_id)}
            >
              <span>{block.state}</span>
            </button>
          ))}
        </div>
      </section>

      <section className="two-column-grid">
        <div className="panel">
          <div className="section-head">
            <h3>Experiment history</h3>
            <div className="button-row">
              <button type="button" className="subtle-button" onClick={loadExperiments} disabled={loadingExperiments || clearingExperiments}>
                Refresh
              </button>
              <button
                type="button"
                className="danger-button"
                onClick={handleClearExperiments}
                disabled={experiments.length === 0 || loadingExperiments || clearingExperiments}
              >
                {clearingExperiments ? 'Clearing…' : 'Clear history'}
              </button>
            </div>
          </div>

          {loadingExperiments ? (
            <div className="state-box info">Loading experiments…</div>
          ) : historyError ? (
            <div className="state-box error" role="alert">{historyError}</div>
          ) : experiments.length === 0 ? (
            <div className="state-box empty">No saved experiments yet. Run a workload to create one.</div>
          ) : (
            <>
            <div className="history-toolbar">
              <span>{experiments.length} saved {experiments.length === 1 ? 'experiment' : 'experiments'}</span>
              <span>{compareIds.length}/4 selected for comparison</span>
            </div>
            <div className="table-wrap history-table-wrap">
              <table>
                <thead>
                  <tr>
                    <th><span className="sr-only">Compare</span></th>
                    <th>Experiment</th>
                    <th>Run status</th>
                    <th>Configuration</th>
                    <th>Workload</th>
                    <th>Performance</th>
                  </tr>
                </thead>
                <tbody>
                  {experiments.map((experiment) => {
                    const successRate = toNumber(experiment.allocation_success_rate ?? experiment.allocation_metrics?.success_rate ?? 0, 0);
                    const failedOperations = toNumber(experiment.failed_operations ?? 0);
                    const runStatus = failedOperations > 0 ? 'partial' : 'complete';
                    const compareSelected = compareIds.includes(experiment.experiment_id);
                    return (
                      <tr
                        key={experiment.experiment_id}
                        className={selectedExperimentId === experiment.experiment_id ? 'selected-row' : ''}
                        onClick={() => setSelectedExperimentId(experiment.experiment_id)}
                        onKeyDown={(event) => {
                          if (event.key === 'Enter' || event.key === ' ') {
                            event.preventDefault();
                            setSelectedExperimentId(experiment.experiment_id);
                          }
                        }}
                        tabIndex={0}
                        aria-selected={selectedExperimentId === experiment.experiment_id}
                      >
                        <td onClick={(event) => event.stopPropagation()}>
                          <input
                            type="checkbox"
                            aria-label={`Compare ${experiment.name || experiment.experiment_id}`}
                            checked={compareSelected}
                            disabled={!compareSelected && compareIds.length >= 4}
                            onChange={() => handleToggleCompare(experiment.experiment_id)}
                          />
                        </td>
                        <td>
                          <div className="experiment-name">{experiment.name || 'Unnamed experiment'}</div>
                          <small>{experiment.experiment_id}</small>
                          <small className="timestamp">{experiment.timestamp ? new Date(experiment.timestamp).toLocaleString() : 'Timestamp unavailable'}</small>
                        </td>
                        <td>
                          <span className={`status-chip ${runStatus === 'complete' ? 'stable' : 'watch'}`}>
                            <span className="status-dot" />{runStatus === 'complete' ? 'Complete' : `${failedOperations} operation failures`}
                          </span>
                          <small className="table-submetric">{formatPercent(successRate * 100, 1)} allocation success</small>
                        </td>
                        <td>
                          <strong>{(experiment.strategy || 'first_fit').replaceAll('_', ' ')}</strong>
                          <small className="table-submetric">{experiment.allocation_mode || 'variable_size'} · {formatNumber(experiment.pool_capacity, 0)} B pool</small>
                          {experiment.allocation_mode === 'fixed_size' && experiment.fixed_block_size > 0
                            && <small className="table-submetric">{formatNumber(experiment.fixed_block_size, 0)} B fixed block</small>}
                        </td>
                        <td>
                          <strong>{experiment.workload_configuration || 'Custom workload'}</strong>
                          <small className="table-submetric">{formatNumber(experiment.total_operations, 0)} operations</small>
                        </td>
                        <td>
                          <strong>{formatNumber(experiment.execution_time_ms, 2)} ms</strong>
                          <small className="table-submetric">{formatNumber(experiment.average_allocation_latency_ms, 3)} ms avg alloc · {formatPercent(experiment.peak_memory_utilization, 2)} peak</small>
                        </td>
                      </tr>
                    );
                  })}
                </tbody>
              </table>
            </div>
            </>
          )}
          <div className="allocation-details-panel">
            <div className="section-head">
              <div>
                <h3>Allocated memory space</h3>
                <p className="muted-note">Live block details from the allocator state.</p>
              </div>
              <span className="status-chip stable">{allocatedBlocks.length} active</span>
            </div>
            {allocatedBlocks.length === 0 ? (
              <div className="state-box empty">No memory blocks are currently allocated.</div>
            ) : (
              <div className="table-wrap allocation-details-table">
                <table>
                  <thead>
                    <tr>
                      <th>Block</th>
                      <th>Allocation</th>
                      <th>Offset</th>
                      <th>Requested</th>
                      <th>Reserved</th>
                      <th>Internal overhead</th>
                      <th>State</th>
                    </tr>
                  </thead>
                  <tbody>
                    {allocatedBlocks.map((block) => {
                      const requestedSize = toNumber(block.requested_size, block.size);
                      const reservedSize = toNumber(block.size);
                      return (
                        <tr key={block.allocation_id ?? block.id} className={selectedAllocationId === block.allocation_id ? 'selected-row' : ''}>
                          <td>{block.id ?? '—'}</td>
                          <td>{block.allocation_id ?? '—'}</td>
                          <td>{formatNumber(block.offset, 0)} B</td>
                          <td>{formatNumber(requestedSize, 0)} B</td>
                          <td>{formatNumber(reservedSize, 0)} B</td>
                          <td>{formatNumber(Math.max(0, reservedSize - requestedSize), 0)} B</td>
                          <td><span className="status-chip stable"><span className="status-dot" />Allocated</span></td>
                        </tr>
                      );
                    })}
                  </tbody>
                </table>
              </div>
            )}
          </div>
        </div>

        <div className="panel detail-panel">
          <div className="section-head">
            <h3>Experiment detail</h3>
            {selectedExperiment && (
              <div className="button-row">
                <button type="button" className="subtle-button" onClick={() => handleToggleCompare(selectedExperiment.experiment_id)}>
                  {compareIds.includes(selectedExperiment.experiment_id) ? 'Remove compare' : 'Add compare'}
                </button>
                <select
                  id="experiment-export-format"
                  aria-label="Experiment CSV contents"
                  value={experimentExportFormat}
                  onChange={(event) => setExperimentExportFormat(event.target.value)}
                >
                  <option value="full">Complete results</option>
                  <option value="operations">Operation log</option>
                  <option value="memory">Memory statistics</option>
                  <option value="fragmentation">Fragmentation metrics</option>
                  <option value="benchmarks">Benchmark results</option>
                  <option value="configuration">Configuration</option>
                  <option value="summary">Summary</option>
                </select>
                <button type="button" className="subtle-button" onClick={handleExportExperiment}>
                  Export experiment CSV
                </button>
              </div>
            )}
          </div>

          {!selectedExperiment ? (
            <div className="state-box empty">No experiment selected.</div>
          ) : loadingDetail ? (
            <div className="state-box info">Loading experiment details…</div>
          ) : detailError ? (
            <div className="state-box error" role="alert">{detailError}</div>
          ) : !summaryExperiment ? (
            <div className="state-box empty">No detail record is available for this experiment.</div>
          ) : (
            <>
              <div className="detail-header">
                <div>
                  <p className="eyebrow">Selected experiment</p>
                  <h4>{selectedExperiment.name || 'Unnamed experiment'}</h4>
                </div>
                <span className={`status-chip ${toNumber(summaryExperiment.workload_metrics?.failed_operations) === 0 ? 'stable' : 'watch'}`}>
                  <span className="status-dot" />
                  {toNumber(summaryExperiment.workload_metrics?.failed_operations) === 0 ? 'Completed' : `${summaryExperiment.workload_metrics?.failed_operations} operation failures`}
                </span>
              </div>

              <div className="detail-grid">
                <div className="detail-section">
                  <h5>Configuration</h5>
                  <dl>
                    <div><dt>ID</dt><dd>{summaryExperiment.experiment_id}</dd></div>
                    <div><dt>Strategy</dt><dd>{summaryExperiment.strategy}</dd></div>
                    <div><dt>Mode</dt><dd>{summaryExperiment.allocation_mode || 'variable_size'}</dd></div>
                    <div><dt>Pool capacity</dt><dd>{formatNumber(summaryExperiment.pool_capacity, 0)}</dd></div>
                    {summaryExperiment.allocation_mode === 'fixed_size' && (
                      <div><dt>Fixed block</dt><dd>{formatNumber(summaryExperiment.fixed_block_size, 0)}</dd></div>
                    )}
                    <div><dt>Random seed</dt><dd>{summaryExperiment.random_seed ?? 0}</dd></div>
                    <div><dt>Timestamp</dt><dd>{summaryExperiment.timestamp ? new Date(summaryExperiment.timestamp).toLocaleString() : '—'}</dd></div>
                    <div><dt>Workload</dt><dd>{summaryExperiment.workload_configuration || 'Custom workload'}</dd></div>
                  </dl>
                </div>

                <div className="detail-section">
                  <h5>Performance</h5>
                  <dl>
                    <div><dt>Execution</dt><dd>{formatNumber(summaryExperiment.workload_metrics?.workload_execution_duration_ms ?? summaryExperiment.execution_time_ms ?? 0, 2)} ms</dd></div>
                    <div><dt>Total allocation time</dt><dd>{formatNumber(summaryExperiment.allocation_metrics?.total_time_ms ?? 0, 3)} ms</dd></div>
                    <div><dt>Avg latency</dt><dd>{formatNumber(summaryExperiment.allocation_metrics?.average_latency_ms ?? summaryExperiment.average_allocation_latency_ms ?? 0, 3)} ms</dd></div>
                    <div><dt>Min / max latency</dt><dd>{formatNumber(summaryExperiment.allocation_metrics?.min_latency_ms ?? 0, 3)} / {formatNumber(summaryExperiment.allocation_metrics?.max_latency_ms ?? 0, 3)} ms</dd></div>
                    <div><dt>P95</dt><dd>{formatNumber(summaryExperiment.allocation_metrics?.p95_latency_ms ?? summaryExperiment.p95_allocation_latency_ms ?? 0, 3)} ms</dd></div>
                    <div><dt>P99</dt><dd>{formatNumber(summaryExperiment.allocation_metrics?.p99_latency_ms ?? 0, 3)} ms</dd></div>
                    <div><dt>Success rate</dt><dd>{formatPercent((summaryExperiment.allocation_metrics?.success_rate ?? 0) * 100, 1)}</dd></div>
                    <div><dt>Failure rate</dt><dd>{formatPercent((summaryExperiment.allocation_metrics?.failure_rate ?? 0) * 100, 1)}</dd></div>
                    <div><dt>Peak util</dt><dd>{formatPercent(summaryExperiment.memory_metrics?.peak_memory_utilization ?? summaryExperiment.peak_memory_utilization ?? 0, 1)}</dd></div>
                    <div><dt>Average util</dt><dd>{formatPercent(summaryExperiment.memory_metrics?.average_memory_utilization ?? 0, 1)}</dd></div>
                  </dl>
                </div>
              </div>

              <div className="metric-card-row">
                {renderMetric('Requested now', formatNumber(summaryExperiment.memory_metrics?.total_requested_memory ?? 0, 0))}
                {renderMetric('Pool consumed', formatNumber(summaryExperiment.memory_metrics?.total_allocated_memory ?? 0, 0))}
                {renderMetric('Peak memory', formatNumber(summaryExperiment.memory_metrics?.peak_memory_usage ?? 0, 0))}
                {renderMetric('Fragmentation', formatPercent(summaryExperiment.fragmentation?.internal_fragmentation_percent ?? summaryExperiment.internal_fragmentation_percent ?? 0, 1))}
                {renderMetric('Largest free', formatNumber(summaryExperiment.fragmentation?.largest_free_block ?? summaryExperiment.largest_free_block ?? 0, 0))}
                {renderMetric('Free blocks', formatNumber(summaryExperiment.fragmentation?.total_free_blocks ?? summaryExperiment.free_block_count ?? 0, 0))}
              </div>

              <div className="detail-section">
                <h5>Workload and operation stats</h5>
                <div className="table-wrap compact">
                  <table>
                    <tbody>
                      <tr><th>Total ops</th><td>{summaryExperiment.workload_metrics?.total_operations ?? summaryExperiment.total_operations ?? 0}</td></tr>
                      <tr><th>Allocations</th><td>{summaryExperiment.workload_metrics?.allocation_operations ?? summaryExperiment.allocation_operations ?? 0}</td></tr>
                      <tr><th>Deallocations</th><td>{summaryExperiment.workload_metrics?.deallocation_operations ?? summaryExperiment.deallocation_operations ?? 0}</td></tr>
                      <tr><th>Successful ops</th><td>{summaryExperiment.workload_metrics?.successful_operations ?? summaryExperiment.successful_operations ?? 0}</td></tr>
                      <tr><th>Failed ops</th><td>{summaryExperiment.workload_metrics?.failed_operations ?? summaryExperiment.failed_operations ?? 0}</td></tr>
                      <tr><th>Peak active allocations</th><td>{summaryExperiment.workload_metrics?.peak_active_allocations ?? 0}</td></tr>
                      <tr><th>Total bytes requested</th><td>{formatNumber(summaryExperiment.workload_metrics?.total_memory_requested_throughout_workload ?? 0, 0)}</td></tr>
                    </tbody>
                  </table>
                </div>
              </div>
              <div className="detail-section">
                <h5>Allocation size distribution</h5>
                {summaryExperiment.workload_metrics?.allocation_size_distribution?.length ? (
                  <div className="distribution-list">
                    {summaryExperiment.workload_metrics.allocation_size_distribution.map((entry) => (
                      <span key={entry.size} className="distribution-chip">{formatNumber(entry.size, 0)} B <strong>× {entry.count}</strong></span>
                    ))}
                  </div>
                ) : <p className="muted-note">No allocation-size samples were recorded.</p>}
              </div>

              <div className="detail-section">
                <h5>Deallocation performance</h5>
                <div className="table-wrap compact">
                  <table>
                    <tbody>
                      <tr><th>Requests</th><td>{summaryExperiment.deallocation_metrics?.total_requests ?? 0}</td></tr>
                      <tr><th>Successful / failed</th><td>{summaryExperiment.deallocation_metrics?.successful_requests ?? 0} / {summaryExperiment.deallocation_metrics?.failed_requests ?? 0}</td></tr>
                      <tr><th>Total time</th><td>{formatNumber(summaryExperiment.deallocation_metrics?.total_time_ms ?? 0, 3)} ms</td></tr>
                      <tr><th>Average latency</th><td>{formatNumber(summaryExperiment.deallocation_metrics?.average_latency_ms ?? 0, 3)} ms</td></tr>
                      <tr><th>Maximum latency</th><td>{formatNumber(summaryExperiment.deallocation_metrics?.max_latency_ms ?? 0, 3)} ms</td></tr>
                    </tbody>
                  </table>
                </div>
              </div>

              <div className="detail-section">
                <h5>Fragmentation</h5>
                <div className="table-wrap compact">
                  <table>
                    <tbody>
                      <tr><th>Internal fragmentation</th><td>{formatPercent(summaryExperiment.fragmentation?.internal_fragmentation_percent ?? summaryExperiment.internal_fragmentation_percent ?? 0, 1)}</td></tr>
                      <tr><th>External fragmentation</th><td>{formatPercent(summaryExperiment.fragmentation?.external_fragmentation_ratio ?? summaryExperiment.external_fragmentation_ratio ?? 0, 1)}</td></tr>
                      <tr><th>Largest free block</th><td>{formatNumber(summaryExperiment.fragmentation?.largest_free_block ?? summaryExperiment.largest_free_block ?? 0, 0)}</td></tr>
                      <tr><th>Avg free block</th><td>{formatNumber(summaryExperiment.fragmentation?.average_free_block_size ?? summaryExperiment.average_free_block_size ?? 0, 2)}</td></tr>
                      <tr><th>Block splits</th><td>{formatNumber(summaryExperiment.fragmentation?.block_splitting_operations ?? summaryExperiment.block_splitting_operations ?? 0, 0)}</td></tr>
                      <tr><th>Coalescing ops</th><td>{formatNumber(summaryExperiment.fragmentation?.successful_coalescing_operations ?? summaryExperiment.successful_coalescing_operations ?? 0, 0)}</td></tr>
                    </tbody>
                  </table>
                </div>
              </div>

              <div className="detail-section">
                <h5>Saved benchmark results</h5>
                {Array.isArray(summaryExperiment.benchmark_results) && summaryExperiment.benchmark_results.length > 0 ? (
                  <div className="table-wrap compact">
                    <table>
                      <thead>
                        <tr>
                          <th>Strategy</th>
                          <th>Execution</th>
                          <th>Avg / P95 / P99 latency</th>
                          <th>Allocation success</th>
                          <th>Peak utilization</th>
                          <th>Fragmentation</th>
                        </tr>
                      </thead>
                      <tbody>
                        {summaryExperiment.benchmark_results.map((result) => (
                          <tr key={`${result.experiment_id}-${result.strategy}`}>
                            <td>{result.strategy.replaceAll('_', ' ')}</td>
                            <td>{formatNumber(result.execution_time_ms, 3)} ms</td>
                            <td>{formatNumber(result.average_allocation_latency_ms, 4)} / {formatNumber(result.p95_allocation_latency_ms, 4)} / {formatNumber(result.p99_allocation_latency_ms, 4)} ms</td>
                            <td>{formatPercent(toNumber(result.allocation_success_rate) * 100, 1)} ({result.successful_allocations}/{result.allocation_requests})</td>
                            <td>{formatPercent(result.peak_memory_utilization, 1)}</td>
                            <td>{formatPercent(result.internal_fragmentation, 1)} internal · {formatPercent(result.external_fragmentation, 1)} external</td>
                          </tr>
                        ))}
                      </tbody>
                    </table>
                  </div>
                ) : (
                  <div className="state-box empty">No benchmark result is attached to this experiment.</div>
                )}
              </div>

              <div className="detail-section">
                <h5>Operation log</h5>
                {Array.isArray(summaryExperiment.operation_sequence) && summaryExperiment.operation_sequence.length > 0 ? (
                  <div className="operation-list">
                    {summaryExperiment.operation_sequence.map((item, index) => (
                      <div key={`${item.operation}-${index}`} className={`operation-item ${item.success ? 'ok' : 'error'}`}>
                        <strong>{item.operation}</strong>
                        <span>{item.success ? 'Succeeded' : 'Failed'}</span>
                        <span>{item.operation === 'allocate' ? `${item.requested_size} B requested · ${item.actual_size} B consumed` : `Allocation ${item.allocation_id}`}</span>
                        <span>{formatNumber(item.latency_ms, 4)} ms</span>
                        {!item.success && item.error_message && <span className="operation-error">{item.error_message}</span>}
                      </div>
                    ))}
                  </div>
                ) : (
                  <div className="state-box empty">No detailed operation log is available for this experiment.</div>
                )}
              </div>
              {summaryExperiment.errors?.length > 0 && (
                <div className="detail-section">
                  <h5>Recorded errors</h5>
                  <ul className="error-list">{summaryExperiment.errors.map((error, index) => <li key={`${index}-${error}`}>{error}</li>)}</ul>
                </div>
              )}
            </>
          )}
        </div>
      </section>

      <section className="panel analytics-panel">
        <div className="section-head">
          <div>
            <h3>Workload Comparison Analytics</h3>
            <p className="muted-note">Visualize actual metrics from the experiments selected above. Missing backend metrics are shown as unavailable rather than estimated.</p>
          </div>
          <div className="button-row">
            <button type="button" className="subtle-button" onClick={handleRefreshComparison} disabled={loadingComparison || compareIds.length < 2}>
              {loadingComparison ? 'Refreshing…' : 'Refresh graphs'}
            </button>
            <button type="button" className="subtle-button" onClick={handleExportComparison} disabled={!comparisonCsv}>Export CSV</button>
            <button type="button" className="subtle-button" onClick={handleClearComparison} disabled={compareIds.length === 0}>Clear selection</button>
          </div>
        </div>
        <div className="analytics-summary">
          <span><strong>{compareIds.length}</strong> selected</span>
          <span>Strategies: {selectedComparisonExperiments.map((item) => item.strategy?.replaceAll('_', ' ')).filter(Boolean).join(', ') || '—'}</span>
          <span>Modes: {selectedComparisonExperiments.map((item) => item.allocation_mode?.replaceAll('_', ' ')).filter(Boolean).join(', ') || '—'}</span>
          <span>Workload conditions: {comparisonConditionsMatch === true ? 'Matching' : comparisonConditionsMatch === false || locallyDetectedConditionDifferences.length ? 'Different' : 'Not evaluated'}</span>
        </div>
        <div className="analytics-controls">
          <label>Focus metric
            <select value={graphMetric} onChange={(event) => setGraphMetric(event.target.value)}>
              {COMPARISON_METRICS.map((metric) => <option key={metric.key} value={metric.key}>{metric.label}</option>)}
            </select>
          </label>
          <label>Chart type
            <select value={graphType} onChange={(event) => setGraphType(event.target.value)}>
              <option value="bar">Grouped bar</option>
              <option value="line">Line trend</option>
            </select>
          </label>
          <label className="checkbox-label"><input type="checkbox" checked={showGraphLegend} onChange={(event) => setShowGraphLegend(event.target.checked)} /> Show legend</label>
          <label className="checkbox-label"><input type="checkbox" checked={showGraphValues} onChange={(event) => setShowGraphValues(event.target.checked)} /> Show values</label>
          {selectedGraphMetric && <span className="muted-note">Focus: {selectedGraphMetric.label} ({selectedGraphMetric.unit})</span>}
        </div>
        {comparisonError ? (
          <div className="state-box error" role="alert">
            {comparisonError}
            <button type="button" className="subtle-button" onClick={handleRefreshComparison}>Retry comparison</button>
          </div>
        ) : comparisonResults.length === 0 ? (
          <div className="state-box empty">Select at least two experiments and run Compare selected to load workload analytics.</div>
        ) : (
          <>
            {comparisonConditionsMatch === false && (
              <div className="state-box warning">These experiments use different workload configurations. Performance differences may not be caused solely by the allocation strategy.</div>
            )}
            <div className="analytics-focus-chart">
              {renderWorkloadGraph({ key: 'focus-metric', title: `${selectedGraphMetric?.label || 'Selected metric'} focus`, metrics: [graphMetric] })}
            </div>
            <div className="analytics-chart-grid">{WORKLOAD_GRAPH_GROUPS.map(renderWorkloadGraph)}</div>
          </>
        )}
      </section>

      <section className="panel comparison-panel">
        <div className="section-head">
          <h3>Side-by-side comparison</h3>
          <button type="button" className="primary-button" onClick={handleCompareSelected} disabled={loadingComparison || compareIds.length < 2}>
            {loadingComparison ? 'Comparing…' : 'Compare selected'}
          </button>
        </div>
        <p className="muted-note">Comparisons are most meaningful when experiments share the same workload and pool configuration. The dashboard highlights relative deltas, not universal superiority.</p>

        {compareIds.length > 0 && (
          <div className="selected-experiments">
            {selectedComparisonExperiments.map((experiment) => (
              <span key={experiment.experiment_id} className="selected-experiment-chip">
                {experiment.name || experiment.strategy} <button type="button" aria-label={`Remove ${experiment.name || experiment.experiment_id} from comparison`} onClick={() => handleToggleCompare(experiment.experiment_id)}>×</button>
              </span>
            ))}
          </div>
        )}
        {comparisonError && <div className="state-box error" role="alert">{comparisonError}</div>}
        {locallyDetectedConditionDifferences.length > 0 && (
          <div className="state-box warning">
            Selected runs differ in {locallyDetectedConditionDifferences.join(', ').toLowerCase()}. The comparison will be shown with a conditions warning.
          </div>
        )}
        {comparisonConditionsMatch === false && comparisonConditionDifferences.length > 0 && (
          <div className="state-box warning">
            <strong>Conditions differ:</strong> {comparisonConditionDifferences.join('; ')}. Treat the figures as descriptive rather than a controlled algorithm comparison.
          </div>
        )}
        {comparisonConditionsMatch === true && comparisonResults.length > 1 && (
          <div className="state-box compatible">
            Workload sequence, pool capacity, allocation mode, fixed block size, and seed match. Differences shown are relative to the first selected experiment.
          </div>
        )}

        {!comparisonResults.length ? (
          <div className="state-box empty">
            {compareIds.length < 2 ? 'Select at least two experiments from the history list to begin a comparison.' : 'Run Compare selected to load the saved measurements.'}
          </div>
        ) : (
          <>
            <div className="comparison-metric-grid">
              {comparisonChartRows.map((row) => {
                const baseline = comparisonChartRows[0] ?? row;
                const executionMetric = COMPARISON_METRICS.find((metric) => metric.key === 'execution_time_ms');
                const latencyMetric = COMPARISON_METRICS.find((metric) => metric.key === 'average_allocation_latency_ms');
                const executionDelta = getMetricDelta(row.execution_time_ms, baseline.execution_time_ms, executionMetric);
                const latencyDelta = getMetricDelta(row.average_allocation_latency_ms, baseline.average_allocation_latency_ms, latencyMetric);
                return (
                  <div key={row.experiment_id || `${row.strategy}-${row.allocation_mode}`} className="comparison-card">
                    <div className="comparison-header">
                      <div>
                        <h4>{row.name || row.strategy}</h4>
                        <small>{row.strategy.replaceAll('_', ' ')} · {row.allocation_mode}</small>
                      </div>
                      <span className={`status-chip ${getTone(row.successRate / 100)}`}>{formatPercent(row.successRate, 1)} success</span>
                    </div>
                    <div className="comparison-chart">
                      <div>
                        <div className="chart-label"><span>Execution time</span><strong>{formatNumber(row.execution_time_ms, 2)} ms</strong></div>
                        <div className="bar-track"><div className="bar-fill" style={{ width: `${row.execWidth}%` }} /></div>
                      </div>
                      <div>
                        <div className="chart-label"><span>Avg allocation latency</span><strong>{formatNumber(row.average_allocation_latency_ms, 3)} ms</strong></div>
                        <div className="bar-track"><div className="bar-fill secondary" style={{ width: `${row.latencyWidth}%` }} /></div>
                      </div>
                    </div>
                    <ul>
                      <li>Execution vs baseline: {row === baseline ? <span className="baseline-label">Baseline</span> : executionDelta && <span className={`delta-pill ${executionDelta.unchanged ? 'neutral' : executionDelta.improved ? 'up' : 'down'}`}>{executionDelta.label} {executionDelta.unchanged ? 'unchanged' : executionDelta.improved ? 'faster' : 'slower'}</span>}</li>
                      <li>Latency vs baseline: {row === baseline ? <span className="baseline-label">Baseline</span> : latencyDelta && <span className={`delta-pill ${latencyDelta.unchanged ? 'neutral' : latencyDelta.improved ? 'up' : 'down'}`}>{latencyDelta.label} {latencyDelta.unchanged ? 'unchanged' : latencyDelta.improved ? 'lower' : 'higher'}</span>}</li>
                    </ul>
                  </div>
                );
              })}
            </div>

            <div className="table-wrap comparison-table">
              <table>
                <thead>
                  <tr>
                    <th>Metric</th>
                    {comparisonResults.map((row) => (
                      <th key={row.experiment_id || `${row.strategy}-${row.allocation_mode}`}>
                        {row.name || row.strategy}
                        {row === comparisonResults[0] && <small className="baseline-heading">Baseline</small>}
                      </th>
                    ))}
                  </tr>
                </thead>
                <tbody>
                  {COMPARISON_METRICS.map((metric) => (
                    <tr key={metric.key}>
                      <th scope="row">{metric.label}</th>
                      {comparisonResults.map((row, index) => {
                        const value = row[metric.key];
                        const delta = index === 0 ? null : getMetricDelta(value, comparisonResults[0][metric.key], metric);
                        return (
                          <td key={row.experiment_id || `${row.strategy}-${metric.key}`}>
                            <strong>{formatMetricValue(metric, value)}</strong>
                            {index === 0 ? <small className="baseline-label">Baseline</small> : delta && (
                              <small className={`metric-delta ${delta.improved === null ? 'neutral' : delta.improved ? 'improved' : 'regressed'}`}>
                                {delta.label}{delta.unchanged ? ' unchanged' : delta.improved === null ? ' vs baseline' : delta.improved ? ' better' : ' worse'}
                              </small>
                            )}
                          </td>
                        );
                      })}
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          </>
        )}
      </section>

      <section className="bottom-grid">
        <div className="panel">
          <h3>Workload editor</h3>
          <label htmlFor="workload-editor">Editable example workload (not an executed result)</label>
          <textarea id="workload-editor" value={workloadText} onChange={(event) => setWorkloadText(event.target.value)} rows={12} />
          <div className="random-workload-controls">
            <h4>Seeded random workload</h4>
            <div className="random-workload-grid">
              <label>Operations<input type="number" min="1" max="10000" value={randomOperationCount} onChange={(event) => setRandomOperationCount(event.target.value)} /></label>
              <label>Minimum bytes<input type="number" min="1" value={randomMinimumSize} onChange={(event) => setRandomMinimumSize(event.target.value)} /></label>
              <label>Maximum bytes<input type="number" min="1" value={randomMaximumSize} onChange={(event) => setRandomMaximumSize(event.target.value)} /></label>
              <label>Allocation probability (%)<input type="number" min="0" max="100" value={randomAllocationProbability} onChange={(event) => setRandomAllocationProbability(event.target.value)} /></label>
              <label>Deallocation probability (%)<input type="number" min="0" max="100" value={randomDeallocationProbability} onChange={(event) => setRandomDeallocationProbability(event.target.value)} /></label>
              <label>Seed<input type="number" min="0" value={randomSeed} onChange={(event) => setRandomSeed(event.target.value)} /></label>
              <label>Allocation algorithm
                <select value={randomStrategy} onChange={(event) => setRandomStrategy(event.target.value)}>
                  <option value="first_fit">First-Fit</option>
                  <option value="best_fit">Best-Fit</option>
                </select>
              </label>
              <label>Allocation mode
                <select value={randomAllocationMode} onChange={(event) => setRandomAllocationMode(event.target.value)}>
                  <option value="variable_size">Variable-size</option>
                  <option value="fixed_size">Fixed-size</option>
                </select>
              </label>
              {randomAllocationMode === 'fixed_size' && (
                <label>Fixed block size (bytes)
                  <input
                    type="number"
                    min="1"
                    max={poolData?.capacity ?? poolCapacity}
                    value={randomFixedBlockSize}
                    onChange={(event) => setRandomFixedBlockSize(event.target.value)}
                  />
                </label>
              )}
            </div>
            <p className="random-workload-selection">
              Selected: <strong>{randomStrategy === 'first_fit' ? 'First-Fit' : 'Best-Fit'}</strong>
              {' / '}
              <strong>{randomAllocationMode === 'fixed_size' ? `Fixed-size (${randomFixedBlockSize} bytes)` : 'Variable-size'}</strong>
            </p>
            {randomWorkloadError && <p className="random-workload-error" role="alert">{randomWorkloadError}</p>}
            {randomExecutionConfiguration && (
              <p className="muted-note" role="status">
                Last executed: {randomExecutionConfiguration.strategy === 'best_fit' ? 'Best-Fit' : 'First-Fit'}
                {' / '}{randomExecutionConfiguration.allocation_mode === 'fixed_size'
                  ? `Fixed-size (${randomExecutionConfiguration.fixed_block_size} bytes)`
                  : 'Variable-size'}
              </p>
            )}
            <button type="button" onClick={handleRunRandomWorkload} disabled={loading || !poolData || Boolean(randomWorkloadError)}>Generate and run from backend</button>
            <p className="muted-note">The selected algorithm and allocation mode are validated and applied by the C++ backend. The seed and workload settings are saved with the experiment.</p>
          </div>
          <div className="button-row">
            <button type="button" onClick={handleStartWorkloadSession} disabled={loading || !poolData}>Start stepwise</button>
            <button type="button" onClick={handleWorkloadStep} disabled={!workloadSession || !['paused', 'running'].includes(workloadSession.status)}>Step</button>
            <button type="button" onClick={handleWorkloadPause} disabled={!workloadSession || workloadSession.status !== 'running'}>Pause</button>
            <button type="button" onClick={handleWorkloadResume} disabled={!workloadSession || workloadSession.status !== 'paused'}>Resume</button>
            <button type="button" className="subtle-button" onClick={handleWorkloadReset} disabled={!workloadSession || workloadSession.status === 'idle'}>Reset session</button>
            <button type="button" onClick={handleRunWorkload} disabled={loading || !poolData}>Run workload</button>
            <button type="button" onClick={handleExportCsv} disabled={loading}>Export CSV</button>
            <button type="button" onClick={handleDownloadCsvTemplate} disabled={loading}>Download CSV template</button>
            <label className="file-button">
              Import CSV
              <input type="file" accept=".csv,text/csv" onChange={handleImportCsv} />
            </label>
          </div>
          {workloadSession && (
            <div className="workload-progress" role="status">
              <div className="section-head">
                <strong>Stepwise session: {workloadSession.status}</strong>
                <span>{workloadSession.current_operation} / {workloadSession.total_operations} operations</span>
              </div>
              <progress value={workloadSession.progress ?? 0} max="100" aria-label="Workload progress" />
              {workloadSession.results?.length > 0 && (
                <p className="muted-note">Last result: {workloadSession.results[workloadSession.results.length - 1].operation} — {workloadSession.results[workloadSession.results.length - 1].success ? 'succeeded' : 'failed'}</p>
              )}
            </div>
          )}
        </div>

        <div className="panel">
          <h3>Benchmark comparison</h3>
          <div className="button-row benchmark-row">
            <button type="button" onClick={handleBenchmark} disabled={loading || !poolData}>Run benchmark</button>
          </div>
          {benchmarkResults.length ? (
            <div className="table-wrap compact">
              <table>
                <thead>
                  <tr>
                    <th>Strategy</th>
                    <th>Exec</th>
                    <th>Latency</th>
                    <th>Success</th>
                  </tr>
                </thead>
                <tbody>
                  {benchmarkResults.map((result) => (
                    <tr key={`${result.strategy}-${result.allocation_mode || 'mode'}`}>
                      <td>{result.strategy}</td>
                      <td>{formatNumber(result.execution_time_ms, 2)} ms</td>
                      <td>{formatNumber(result.average_allocation_latency_ms, 3)} ms</td>
                      <td>{formatPercent(toNumber(result.allocation_success_rate, 0) * 100, 1)}</td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          ) : (
            <div className="state-box empty">No benchmark results yet.</div>
          )}
        </div>
      </section>

      <section className="bottom-grid">
        <div className="panel">
          <h3>Active allocations</h3>
          {allocatedBlocks.length === 0 ? <div className="state-box empty">No active allocations.</div> : (
            <ul className="allocation-list">
              {allocatedBlocks.map((block) => (
                <li key={block.allocation_id}>
                  <button type="button" onClick={() => setSelectedAllocationId(block.allocation_id)}>
                    Allocation {block.allocation_id} — offset {block.offset}, size {block.size}
                  </button>
                </li>
              ))}
            </ul>
          )}
        </div>

        <div className="panel">
          <h3>Workload results</h3>
          {workloadResults.length === 0 ? <div className="state-box empty">Run a workload to see results here.</div> : (
            <pre>{JSON.stringify(workloadResults, null, 2)}</pre>
          )}
        </div>
      </section>
    </div>
  );
}

export default App;
