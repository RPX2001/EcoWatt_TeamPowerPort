/**
 * CompressionBench Component
 * 
 * Tool for benchmarking compression algorithms
 * Features:
 * - Configurable test parameters
 * - Algorithm comparison
 * - Results visualization
 * - Performance metrics
 */

import React, { useState } from 'react';
import {
  Box,
  Paper,
  Typography,
  Button,
  TextField,
  Alert,
  CircularProgress,
  Divider,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Grid
} from '@mui/material';
import {
  Speed as BenchmarkIcon,
  Refresh as RerunIcon
} from '@mui/icons-material';
import { useMutation } from '@tanstack/react-query';
import { runCompressionBenchmark } from '../../api/utilities';
import StatisticsCard from '../common/StatisticsCard';

const CompressionBench = () => {
  const [dataSize, setDataSize] = useState(1024);
  const [iterations, setIterations] = useState(100);
  const [results, setResults] = useState(null);

  const benchmarkMutation = useMutation({
    mutationFn: () => runCompressionBenchmark(dataSize, iterations),
    onSuccess: (response) => {
      setResults(response.data);
    }
  });

  const handleRunBenchmark = () => {
    benchmarkMutation.mutate();
  };

  const formatTime = (ms) => {
    if (ms === undefined || ms === null) return 'N/A';
    if (ms < 1) return `${(ms * 1000).toFixed(2)} μs`;
    return `${ms.toFixed(2)} ms`;
  };

  const formatSize = (bytes) => {
    if (bytes === undefined || bytes === null) return 'N/A';
    if (bytes < 1024) return `${bytes} B`;
    if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(2)} KB`;
    return `${(bytes / (1024 * 1024)).toFixed(2)} MB`;
  };

  return (
    <Box>
      <Paper sx={{ p: 3 }}>
        <Typography variant="h6" gutterBottom>
          Compression Algorithm Benchmark
        </Typography>
        <Typography variant="body2" color="text.secondary" gutterBottom>
          Compare compression algorithms for performance and efficiency
        </Typography>

        <Divider sx={{ my: 3 }} />

        {/* Configuration */}
        <Grid container spacing={2} sx={{ mb: 3 }}>
          <Grid item xs={12} sm={6}>
            <TextField
              fullWidth
              label="Test Data Size (bytes)"
              type="number"
              value={dataSize}
              onChange={(e) => setDataSize(parseInt(e.target.value) || 1024)}
              helperText="Size of test data for compression"
            />
          </Grid>
          <Grid item xs={12} sm={6}>
            <TextField
              fullWidth
              label="Iterations"
              type="number"
              value={iterations}
              onChange={(e) => setIterations(parseInt(e.target.value) || 100)}
              helperText="Number of test iterations for averaging"
            />
          </Grid>
        </Grid>

        {/* Run Button */}
        <Button
          variant="contained"
          color="primary"
          onClick={handleRunBenchmark}
          disabled={benchmarkMutation.isPending}
          startIcon={benchmarkMutation.isPending ? <CircularProgress size={20} /> : <BenchmarkIcon />}
          fullWidth
          sx={{ mb: 3 }}
        >
          {benchmarkMutation.isPending ? 'Running Benchmark...' : 'Run Benchmark'}
        </Button>

        {/* Error Display */}
        {benchmarkMutation.isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            {benchmarkMutation.error?.response?.data?.error || 'Failed to run benchmark'}
          </Alert>
        )}

        {/* Success Display */}
        {benchmarkMutation.isSuccess && results && (
          <Alert severity="success" sx={{ mb: 2 }}>
            {results.message || 'Benchmark completed successfully!'}
          </Alert>
        )}

        {/* Results Display */}
        {results && results.results && (
          <Box>
            <Divider sx={{ my: 3 }} />
            <Typography variant="h6" gutterBottom>
              Benchmark Results
            </Typography>

            {/* Summary Cards */}
            {results.results.algorithms && (
              <Grid container spacing={2} sx={{ mb: 3 }}>
                {Object.entries(results.results.algorithms).map(([algo, data]) => (
                  <Grid item xs={12} sm={6} md={4} key={algo}>
                    <StatisticsCard
                      title={algo.toUpperCase()}
                      value={`${((data.ratio || 0) * 100).toFixed(1)}%`}
                      subtitle={`Speed: ${formatTime(data.avg_time)}`}
                      icon={BenchmarkIcon}
                      color="primary"
                    />
                  </Grid>
                ))}
              </Grid>
            )}

            {/* Detailed Table */}
            {results.results.algorithms && (
              <TableContainer>
                <Table size="small">
                  <TableHead>
                    <TableRow>
                      <TableCell><strong>Algorithm</strong></TableCell>
                      <TableCell align="right"><strong>Compressed Size</strong></TableCell>
                      <TableCell align="right"><strong>Ratio</strong></TableCell>
                      <TableCell align="right"><strong>Avg Time</strong></TableCell>
                      <TableCell align="right"><strong>Speed</strong></TableCell>
                    </TableRow>
                  </TableHead>
                  <TableBody>
                    {Object.entries(results.results.algorithms).map(([algo, data]) => (
                      <TableRow key={algo}>
                        <TableCell>{algo.toUpperCase()}</TableCell>
                        <TableCell align="right">{formatSize(data.compressed_size)}</TableCell>
                        <TableCell align="right">{((data.ratio || 0) * 100).toFixed(2)}%</TableCell>
                        <TableCell align="right">{formatTime(data.avg_time)}</TableCell>
                        <TableCell align="right">
                          {data.speed ? `${(data.speed / 1024).toFixed(2)} KB/s` : 'N/A'}
                        </TableCell>
                      </TableRow>
                    ))}
                  </TableBody>
                </Table>
              </TableContainer>
            )}

            {/* Raw Output (fallback) */}
            {results.results.raw_output && (
              <Box sx={{ mt: 3 }}>
                <Typography variant="subtitle2" gutterBottom>
                  Benchmark Output:
                </Typography>
                <Paper
                  sx={{
                    p: 2,
                    bgcolor: 'grey.100',
                    fontFamily: 'monospace',
                    fontSize: '0.875rem',
                    overflow: 'auto',
                    maxHeight: 300
                  }}
                >
                  <pre style={{ margin: 0 }}>
                    {results.results.raw_output}
                  </pre>
                </Paper>
              </Box>
            )}

            {/* Parameters Used */}
            {results.parameters && (
              <Box sx={{ mt: 2 }}>
                <Typography variant="caption" color="text.secondary">
                  Test Parameters: {formatSize(results.parameters.data_size)} data, {results.parameters.iterations} iterations
                </Typography>
              </Box>
            )}
          </Box>
        )}

        {/* Instructions */}
        <Box sx={{ mt: 4, p: 2, bgcolor: 'info.lighter', borderRadius: 1 }}>
          <Typography variant="subtitle2" gutterBottom>
            ℹ️ About Compression Benchmarks:
          </Typography>
          <Typography variant="body2" component="div">
            <ul style={{ margin: 0, paddingLeft: '1.5rem' }}>
              <li><strong>Ratio:</strong> Percentage of original size after compression (lower is better)</li>
              <li><strong>Speed:</strong> Average time to compress data (lower is better)</li>
              <li><strong>ZLIB:</strong> Good balance of compression and speed (default)</li>
              <li><strong>LZ4:</strong> Very fast with moderate compression</li>
              <li><strong>GZIP:</strong> Better compression, slower speed</li>
              <li>Test with realistic data sizes for your application</li>
            </ul>
          </Typography>
        </Box>
      </Paper>
    </Box>
  );
};

export default CompressionBench;
