/**
 * ConfigHistory Component
 * 
 * Displays configuration change history
 * Features:
 * - Timeline view of config changes
 * - Diff comparison between versions
 * - Filter by device
 * - Expandable details showing what changed
 */

import React, { useState } from 'react';
import {
  Box,
  Paper,
  Typography,
  Table,
  TableBody,
  TableCell,
  TableContainer,
  TableHead,
  TableRow,
  Chip,
  FormControl,
  InputLabel,
  Select,
  MenuItem,
  Alert,
  CircularProgress,
  Stack,
  IconButton,
  Collapse,
  Grid,
  Divider
} from '@mui/material';
import {
  ExpandMore as ExpandMoreIcon,
  ExpandLess as ExpandLessIcon,
  History as HistoryIcon,
  Compare as CompareIcon
} from '@mui/icons-material';
import { useQuery } from '@tanstack/react-query';
import { getConfigHistory } from '../../api/config';

const ConfigHistory = ({ deviceId }) => {
  const [expandedRows, setExpandedRows] = useState(new Set());

  // Fetch config history
  const {
    data: historyData,
    isLoading,
    isError,
    error
  } = useQuery({
    queryKey: ['configHistory', deviceId],
    queryFn: () => getConfigHistory(deviceId),
    enabled: !!deviceId,
    staleTime: 10000
  });

  const history = historyData?.data?.history || [];

  const toggleRowExpand = (index) => {
    setExpandedRows(prev => {
      const newSet = new Set(prev);
      if (newSet.has(index)) {
        newSet.delete(index);
      } else {
        newSet.add(index);
      }
      return newSet;
    });
  };

  const formatTimestamp = (timestamp) => {
    return new Date(timestamp * 1000).toLocaleString();
  };

  const getChangedFields = (current, previous) => {
    if (!current) return [];
    if (!previous) return Object.keys(current);
    
    const changed = [];
    for (const key in current) {
      if (JSON.stringify(current[key]) !== JSON.stringify(previous[key])) {
        changed.push(key);
      }
    }
    return changed;
  };

  const renderConfigDiff = (current, previous) => {
    if (!current) {
      return (
        <Alert severity="warning" sx={{ mb: 2 }}>
          No configuration data available
        </Alert>
      );
    }

    if (!previous) {
      return (
        <Alert severity="info" sx={{ mb: 2 }}>
          Initial configuration
        </Alert>
      );
    }

    const changedFields = getChangedFields(current, previous);

    if (changedFields.length === 0) {
      return (
        <Alert severity="info" sx={{ mb: 2 }}>
          No changes detected
        </Alert>
      );
    }

    return (
      <Box>
        <Typography variant="subtitle2" gutterBottom sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
          <CompareIcon fontSize="small" />
          Changed Fields ({changedFields.length})
        </Typography>
        
        <Grid container spacing={2} sx={{ mt: 1 }}>
          {changedFields.map((field) => (
            <React.Fragment key={field}>
              <Grid item xs={12}>
                <Box sx={{ p: 2, bgcolor: 'grey.50', borderRadius: 1 }}>
                  <Typography variant="body2" fontWeight="bold" gutterBottom>
                    {field}
                  </Typography>
                  
                  <Grid container spacing={2}>
                    <Grid item xs={6}>
                      <Box sx={{ p: 1, bgcolor: 'error.50', borderRadius: 1, border: '1px solid', borderColor: 'error.light' }}>
                        <Typography variant="caption" color="error.dark" fontWeight="bold">
                          Previous:
                        </Typography>
                        <Typography variant="body2" sx={{ fontFamily: 'monospace', mt: 0.5 }}>
                          {JSON.stringify(previous[field], null, 2)}
                        </Typography>
                      </Box>
                    </Grid>
                    
                    <Grid item xs={6}>
                      <Box sx={{ p: 1, bgcolor: 'success.50', borderRadius: 1, border: '1px solid', borderColor: 'success.light' }}>
                        <Typography variant="caption" color="success.dark" fontWeight="bold">
                          Current:
                        </Typography>
                        <Typography variant="body2" sx={{ fontFamily: 'monospace', mt: 0.5 }}>
                          {JSON.stringify(current[field], null, 2)}
                        </Typography>
                      </Box>
                    </Grid>
                  </Grid>
                </Box>
              </Grid>
            </React.Fragment>
          ))}
        </Grid>
      </Box>
    );
  };

  const renderConfigDetails = (config) => {
    if (!config) {
      return (
        <Alert severity="warning">
          No configuration data available
        </Alert>
      );
    }

    return (
      <Box sx={{ display: 'grid', gridTemplateColumns: '200px 1fr', gap: 1 }}>
        {/* Timing Parameters */}
        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Sampling Interval:
        </Typography>
        <Typography variant="body2">
          {config.sampling_interval || 'N/A'} seconds
        </Typography>

        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Upload Interval:
        </Typography>
        <Typography variant="body2">
          {config.upload_interval || 'N/A'} seconds
        </Typography>

        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Firmware Check Interval:
        </Typography>
        <Typography variant="body2">
          {config.firmware_check_interval || 'N/A'} seconds
        </Typography>

        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Command Poll Interval:
        </Typography>
        <Typography variant="body2">
          {config.command_poll_interval || 'N/A'} seconds
        </Typography>

        <Typography variant="body2" color="text.secondary" fontWeight="bold">
          Config Poll Interval:
        </Typography>
        <Typography variant="body2">
          {config.config_poll_interval || 'N/A'} seconds
        </Typography>

        {/* Modbus Registers */}
        {config.modbus_registers && config.modbus_registers.length > 0 && (
          <>
            <Typography variant="body2" color="text.secondary" fontWeight="bold" sx={{ mt: 1 }}>
              Modbus Registers:
            </Typography>
            <Box sx={{ mt: 1 }}>
              {config.modbus_registers.map((reg, idx) => (
                <Chip 
                  key={idx}
                  label={`Reg ${reg}`}
                  size="small"
                  sx={{ mr: 0.5, mb: 0.5 }}
                />
              ))}
            </Box>
          </>
        )}
      </Box>
    );
  };

  return (
    <Box sx={{ p: 3 }}>
      <Paper sx={{ p: 3 }}>
        {/* Header */}
        <Box sx={{ mb: 3 }}>
          <Stack direction="row" spacing={2} alignItems="center" sx={{ mb: 2 }}>
            <HistoryIcon color="primary" />
            <Typography variant="h5">
              Configuration History
            </Typography>
          </Stack>
        </Box>

        {/* Loading State */}
        {deviceId && isLoading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', py: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {/* Error State */}
        {deviceId && isError && (
          <Alert severity="error" sx={{ mb: 2 }}>
            Error loading configuration history: {error?.message || 'Unknown error'}
          </Alert>
        )}

        {/* No Device Selected */}
        {!deviceId && (
          <Alert severity="info">
            Please select a device to view configuration history.
          </Alert>
        )}

        {/* Empty State */}
        {deviceId && !isLoading && !isError && history.length === 0 && (
          <Alert severity="info">
            No configuration history found for this device
          </Alert>
        )}

        {/* History Table */}
        {deviceId && !isLoading && !isError && history.length > 0 && (
          <TableContainer>
            <Table>
              <TableHead>
                <TableRow>
                  <TableCell width={50}></TableCell>
                  <TableCell>Timestamp</TableCell>
                  <TableCell>Changes</TableCell>
                  <TableCell>Status</TableCell>
                  <TableCell>Acknowledged</TableCell>
                </TableRow>
              </TableHead>
              <TableBody>
                {history.map((entry, index) => {
                  // Backend stores 'config_update', not 'config'
                  const currentConfig = entry.config || entry.config_update;
                  
                  if (!entry || !currentConfig) {
                    return null; // Skip invalid entries
                  }

                  const previousEntry = index < history.length - 1 ? history[index + 1] : null;
                  const previousConfig = previousEntry?.config || previousEntry?.config_update;
                  
                  const changedFields = getChangedFields(
                    currentConfig,
                    previousConfig
                  );

                  return (
                    <React.Fragment key={index}>
                      <TableRow hover>
                        <TableCell>
                          <IconButton
                            size="small"
                            onClick={() => toggleRowExpand(index)}
                          >
                            {expandedRows.has(index) ? (
                              <ExpandLessIcon />
                            ) : (
                              <ExpandMoreIcon />
                            )}
                          </IconButton>
                        </TableCell>
                        <TableCell>
                          {formatTimestamp(entry.timestamp)}
                        </TableCell>
                        <TableCell>
                          {previousEntry ? (
                            <Box>
                              <Typography variant="body2" fontWeight="bold">
                                {changedFields.length} field{changedFields.length !== 1 ? 's' : ''} changed
                              </Typography>
                              {changedFields.length > 0 && changedFields.length <= 3 && (
                                <Typography variant="caption" color="text.secondary">
                                  {changedFields.join(', ')}
                                </Typography>
                              )}
                            </Box>
                          ) : (
                            <Chip label="Initial" size="small" color="primary" />
                          )}
                        </TableCell>
                        <TableCell>
                          <Chip
                            label={entry.status || 'applied'}
                            color="success"
                            size="small"
                          />
                        </TableCell>
                        <TableCell>
                          {entry.acknowledged_at ? (
                            <Chip
                              label={`✓ ${new Date(entry.acknowledged_at * 1000).toLocaleString()}`}
                              color="success"
                              size="small"
                              variant="outlined"
                            />
                          ) : (
                            <Chip
                              label="Pending"
                              color="warning"
                              size="small"
                              variant="outlined"
                            />
                          )}
                        </TableCell>
                      </TableRow>

                      {/* Expanded Details Row */}
                      <TableRow>
                        <TableCell colSpan={4} sx={{ p: 0, border: 0 }}>
                          <Collapse in={expandedRows.has(index)}>
                            <Box sx={{ p: 3, bgcolor: 'grey.50' }}>
                              {/* Diff View */}
                              <Box sx={{ mb: 3 }}>
                                <Typography variant="h6" gutterBottom>
                                  Changes
                                </Typography>
                                {renderConfigDiff(currentConfig, previousConfig)}
                              </Box>

                              <Divider sx={{ my: 2 }} />

                              {/* Full Configuration Update */}
                              <Box>
                                <Typography variant="h6" gutterBottom>
                                  Configuration Update
                                </Typography>
                                {renderConfigDetails(currentConfig)}
                              </Box>
                            </Box>
                          </Collapse>
                        </TableCell>
                      </TableRow>
                    </React.Fragment>
                  );
                })}
              </TableBody>
            </Table>
          </TableContainer>
        )}

        {/* Footer */}
        <Box sx={{ mt: 2 }}>
          <Typography variant="body2" color="text.secondary">
            Showing {history.length} configuration change{history.length !== 1 ? 's' : ''}
          </Typography>
        </Box>
      </Paper>
    </Box>
  );
};

export default ConfigHistory;
