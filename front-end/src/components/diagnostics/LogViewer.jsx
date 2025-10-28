/**
 * LogViewer Component
 * 
 * Displays diagnostic logs with filtering and search
 * Features:
 * - Scrollable log display
 * - Color coding by severity (INFO, WARNING, ERROR)
 * - Auto-scroll to bottom toggle
 * - Search functionality
 * - Export logs to CSV/JSON
 * - Real-time updates
 */

import React, { useState, useEffect, useRef } from 'react';
import {
  Box,
  Paper,
  Typography,
  List,
  ListItem,
  ListItemText,
  CircularProgress,
  Alert,
  Button,
  Switch,
  FormControlLabel,
  Stack,
  Divider,
  Chip
} from '@mui/material';
import {
  Refresh as RefreshIcon,
  Download as DownloadIcon,
  Delete as DeleteIcon
} from '@mui/icons-material';
import { useQuery, useMutation, useQueryClient } from '@tanstack/react-query';
import { getAllDiagnostics, getDeviceDiagnostics, clearAllDiagnostics, clearDeviceDiagnostics } from '../../api/diagnostics';
import LogFilters from './LogFilters';

const LogViewer = () => {
  const [filters, setFilters] = useState({
    deviceId: 'all',
    level: 'all',
    search: '',
    startDate: '',
    endDate: ''
  });
  const [autoScroll, setAutoScroll] = useState(true);
  const [autoRefresh, setAutoRefresh] = useState(false);
  const logEndRef = useRef(null);
  const queryClient = useQueryClient();

  // Fetch diagnostics based on filters
  const {
    data: diagnosticsData,
    isLoading,
    isError,
    refetch
  } = useQuery({
    queryKey: ['diagnostics', filters.deviceId],
    queryFn: () => {
      if (filters.deviceId === 'all') {
        return getAllDiagnostics(100);
      } else {
        return getDeviceDiagnostics(filters.deviceId, 100);
      }
    },
    refetchInterval: autoRefresh ? 5000 : false,
    staleTime: 3000
  });

  // Clear diagnostics mutation
  const clearMutation = useMutation({
    mutationFn: () => {
      if (filters.deviceId === 'all') {
        return clearAllDiagnostics();
      } else {
        return clearDeviceDiagnostics(filters.deviceId);
      }
    },
    onSuccess: () => {
      queryClient.invalidateQueries(['diagnostics']);
    }
  });

  // Auto-scroll to bottom
  useEffect(() => {
    if (autoScroll && logEndRef.current) {
      logEndRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [diagnosticsData, autoScroll]);

  const handleFilterChange = (newFilters) => {
    setFilters(newFilters);
  };

  const handleClearFilters = () => {
    setFilters({
      deviceId: 'all',
      level: 'all',
      search: '',
      startDate: '',
      endDate: ''
    });
  };

  const handleClearLogs = () => {
    if (window.confirm('Are you sure you want to clear all logs?')) {
      clearMutation.mutate();
    }
  };

  const handleExport = (format) => {
    const logs = getFilteredLogs();
    
    if (format === 'json') {
      const dataStr = JSON.stringify(logs, null, 2);
      const dataBlob = new Blob([dataStr], { type: 'application/json' });
      const url = URL.createObjectURL(dataBlob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `diagnostics_${new Date().toISOString()}.json`;
      link.click();
    } else if (format === 'csv') {
      const headers = ['Timestamp', 'Device ID', 'Level', 'Message', 'Details'];
      const csvRows = [headers.join(',')];
      
      logs.forEach(log => {
        const row = [
          log.timestamp,
          log.device_id || 'N/A',
          log.level || 'INFO',
          `"${(log.message || '').replace(/"/g, '""')}"`,
          `"${JSON.stringify(log.data || {}).replace(/"/g, '""')}"`
        ];
        csvRows.push(row.join(','));
      });
      
      const csvStr = csvRows.join('\n');
      const dataBlob = new Blob([csvStr], { type: 'text/csv' });
      const url = URL.createObjectURL(dataBlob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `diagnostics_${new Date().toISOString()}.csv`;
      link.click();
    }
  };

  const getLogColor = (level) => {
    switch (level?.toLowerCase()) {
      case 'error':
        return 'error.main';
      case 'warning':
        return 'warning.main';
      case 'info':
        return 'info.main';
      case 'debug':
        return 'text.secondary';
      default:
        return 'text.primary';
    }
  };

  const getLogIcon = (level) => {
    switch (level?.toLowerCase()) {
      case 'error':
        return '❌';
      case 'warning':
        return '⚠️';
      case 'info':
        return 'ℹ️';
      case 'debug':
        return '🔍';
      default:
        return '📝';
    }
  };

  const getFilteredLogs = () => {
    let logs = [];

    if (filters.deviceId === 'all') {
      const allDiagnostics = diagnosticsData?.data?.diagnostics || {};
      Object.keys(allDiagnostics).forEach(deviceId => {
        const deviceLogs = allDiagnostics[deviceId] || [];
        logs = [...logs, ...deviceLogs.map(log => ({ ...log, device_id: deviceId }))];
      });
    } else {
      logs = diagnosticsData?.data?.diagnostics || [];
    }

    // Sort by timestamp (newest first)
    logs.sort((a, b) => new Date(b.timestamp || 0) - new Date(a.timestamp || 0));

    // Apply filters
    if (filters.level && filters.level !== 'all') {
      logs = logs.filter(log => 
        (log.level || 'info').toLowerCase() === filters.level.toLowerCase()
      );
    }

    if (filters.search) {
      const searchLower = filters.search.toLowerCase();
      logs = logs.filter(log => 
        (log.message || '').toLowerCase().includes(searchLower) ||
        (log.device_id || '').toLowerCase().includes(searchLower) ||
        JSON.stringify(log.data || {}).toLowerCase().includes(searchLower)
      );
    }

    if (filters.startDate) {
      const startTime = new Date(filters.startDate).getTime();
      logs = logs.filter(log => new Date(log.timestamp).getTime() >= startTime);
    }

    if (filters.endDate) {
      const endTime = new Date(filters.endDate).getTime();
      logs = logs.filter(log => new Date(log.timestamp).getTime() <= endTime);
    }

    return logs;
  };

  const filteredLogs = getFilteredLogs();

  return (
    <Box>
      {/* Filters */}
      <LogFilters
        filters={filters}
        onFilterChange={handleFilterChange}
        onClearFilters={handleClearFilters}
      />

      {/* Controls */}
      <Paper sx={{ p: 2, mb: 2 }}>
        <Stack direction="row" spacing={2} alignItems="center" flexWrap="wrap">
          <FormControlLabel
            control={
              <Switch
                checked={autoScroll}
                onChange={(e) => setAutoScroll(e.target.checked)}
              />
            }
            label="Auto-scroll"
          />
          
          <FormControlLabel
            control={
              <Switch
                checked={autoRefresh}
                onChange={(e) => setAutoRefresh(e.target.checked)}
              />
            }
            label="Auto-refresh (5s)"
          />

          <Divider orientation="vertical" flexItem />

          <Button
            size="small"
            startIcon={<RefreshIcon />}
            onClick={() => refetch()}
            disabled={isLoading}
          >
            Refresh
          </Button>

          <Button
            size="small"
            startIcon={<DownloadIcon />}
            onClick={() => handleExport('json')}
            disabled={filteredLogs.length === 0}
          >
            Export JSON
          </Button>

          <Button
            size="small"
            startIcon={<DownloadIcon />}
            onClick={() => handleExport('csv')}
            disabled={filteredLogs.length === 0}
          >
            Export CSV
          </Button>

          <Button
            size="small"
            color="error"
            startIcon={<DeleteIcon />}
            onClick={handleClearLogs}
            disabled={clearMutation.isPending}
          >
            Clear Logs
          </Button>

          <Box sx={{ flexGrow: 1 }} />

          <Chip 
            label={`${filteredLogs.length} logs`} 
            size="small" 
            color="primary"
          />
        </Stack>
      </Paper>

      {/* Log Display */}
      <Paper sx={{ p: 0, maxHeight: 600, overflow: 'auto' }}>
        {isLoading && (
          <Box sx={{ display: 'flex', justifyContent: 'center', p: 4 }}>
            <CircularProgress />
          </Box>
        )}

        {isError && (
          <Alert severity="error" sx={{ m: 2 }}>
            Error loading diagnostics logs
          </Alert>
        )}

        {!isLoading && !isError && filteredLogs.length === 0 && (
          <Alert severity="info" sx={{ m: 2 }}>
            No logs found. Adjust filters or wait for new diagnostic data.
          </Alert>
        )}

        {!isLoading && !isError && filteredLogs.length > 0 && (
          <List sx={{ p: 0 }}>
            {filteredLogs.map((log, index) => (
              <React.Fragment key={index}>
                <ListItem
                  sx={{
                    borderLeft: `4px solid`,
                    borderLeftColor: getLogColor(log.level),
                    '&:hover': { bgcolor: 'action.hover' }
                  }}
                >
                  <ListItemText
                    primary={
                      <Box sx={{ display: 'flex', alignItems: 'center', gap: 1 }}>
                        <Typography component="span" sx={{ fontSize: '1.2rem' }}>
                          {getLogIcon(log.level)}
                        </Typography>
                        <Chip 
                          label={log.level || 'INFO'} 
                          size="small"
                          sx={{ 
                            minWidth: 80,
                            bgcolor: getLogColor(log.level),
                            color: 'white',
                            fontWeight: 'bold'
                          }}
                        />
                        <Typography variant="body2" color="text.secondary">
                          {log.device_id || 'System'}
                        </Typography>
                        <Typography variant="body2" color="text.secondary">
                          •
                        </Typography>
                        <Typography variant="body2" color="text.secondary">
                          {new Date(log.timestamp).toLocaleString()}
                        </Typography>
                      </Box>
                    }
                    secondary={
                      <Box sx={{ mt: 1 }}>
                        <Typography variant="body1" sx={{ color: 'text.primary', mb: 1 }}>
                          {log.message || 'No message'}
                        </Typography>
                        {log.data && Object.keys(log.data).length > 0 && (
                          <Box 
                            sx={{ 
                              bgcolor: 'grey.100', 
                              p: 1, 
                              borderRadius: 1,
                              fontFamily: 'monospace',
                              fontSize: '0.875rem',
                              overflow: 'auto'
                            }}
                          >
                            <pre style={{ margin: 0 }}>
                              {JSON.stringify(log.data, null, 2)}
                            </pre>
                          </Box>
                        )}
                      </Box>
                    }
                  />
                </ListItem>
                {index < filteredLogs.length - 1 && <Divider />}
              </React.Fragment>
            ))}
            <div ref={logEndRef} />
          </List>
        )}
      </Paper>
    </Box>
  );
};

export default LogViewer;
