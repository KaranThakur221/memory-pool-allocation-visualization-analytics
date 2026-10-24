export function validateRandomWorkloadConfiguration(configuration, poolCapacity) {
  const operationCount = Number(configuration.operation_count);
  const minimumSize = Number(configuration.minimum_size);
  const maximumSize = Number(configuration.maximum_size);
  const allocationProbability = Number(configuration.allocation_probability);
  const deallocationProbability = Number(configuration.deallocation_probability);
  const seed = Number(configuration.random_seed);
  const fixedBlockSize = Number(configuration.fixed_block_size);

  if (!Number.isSafeInteger(operationCount) || operationCount < 1 || operationCount > 10000) {
    return 'Operations must be a whole number from 1 to 10,000.';
  }
  if (!Number.isSafeInteger(minimumSize) || minimumSize < 1
      || !Number.isSafeInteger(maximumSize) || maximumSize < minimumSize
      || maximumSize > poolCapacity) {
    return 'Allocation sizes must be whole numbers within the pool capacity, and maximum bytes must be at least minimum bytes.';
  }
  if (!Number.isSafeInteger(allocationProbability) || allocationProbability < 0 || allocationProbability > 100
      || !Number.isSafeInteger(deallocationProbability) || deallocationProbability < 0 || deallocationProbability > 100
      || allocationProbability + deallocationProbability !== 100) {
    return 'Allocation and deallocation probabilities must be whole percentages that total 100%.';
  }
  if (!Number.isSafeInteger(seed) || seed < 0) {
    return 'Seed must be a non-negative safe integer.';
  }
  if (configuration.allocation_mode === 'fixed_size'
      && (!Number.isSafeInteger(fixedBlockSize) || fixedBlockSize < 1
        || fixedBlockSize > poolCapacity || maximumSize > fixedBlockSize)) {
    return 'Fixed block size must be within pool capacity and at least the maximum allocation size.';
  }
  if (!['first_fit', 'best_fit'].includes(configuration.strategy)) {
    return 'Select First-Fit or Best-Fit as the allocation algorithm.';
  }
  if (!['fixed_size', 'variable_size'].includes(configuration.allocation_mode)) {
    return 'Select fixed-size or variable-size allocation mode.';
  }
  return '';
}

export function createRandomWorkloadRequest(configuration) {
  return {
    operation_count: Number(configuration.operation_count),
    minimum_size: Number(configuration.minimum_size),
    maximum_size: Number(configuration.maximum_size),
    allocation_probability: Number(configuration.allocation_probability),
    deallocation_probability: Number(configuration.deallocation_probability),
    random_seed: Number(configuration.random_seed),
    strategy: configuration.strategy,
    allocation_mode: configuration.allocation_mode,
    fixed_block_size: Number(configuration.fixed_block_size),
  };
}
