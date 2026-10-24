# Observation notebook

> **Data provenance warning:** The existing rows in `experiment_events.csv`
> and `issue_log.csv`, and the brief examples below, are legacy illustrative
> fixtures. Their IDs and measurements have not been verified against saved
> backend experiment records and must not be cited as actual observations.
> They are preserved as-is; the scripts currently read from
> `data/benchmark_results/`, not from these fixtures. Only report a measured
> observation after retrieving its actual experiment ID and metrics from the
> backend history.

This notebook describes candidate observation categories; it does not contain
verified quantitative experiment results.

## Candidate experiment: fixed-size allocation

- Objective: verify reuse and utilization on equal-size requests.
- Result: not verified from a saved experiment record.

## Candidate experiment: variable-size allocation

- Objective: confirm variable requests and best-fit behavior.
- Result: not verified from a saved experiment record.

## Interpretation

Expected allocator behavior is documented in
`documentation/FUNCTIONALITY_DOCUMENTATION.md`; do not treat that explanation
as a measured result unless linked to an actual saved experiment.
