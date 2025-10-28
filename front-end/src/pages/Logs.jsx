/**
 * Logs & Diagnostics Page
 * 
 * Main page for viewing diagnostic logs and system statistics
 * Features:
 * - System health summary
 * - Statistics dashboard (compression, security, OTA, commands)
 * - Real-time metrics
 */

import React, { useState } from 'react';
import {
  Box,
  Container,
  Typography,
  Grid,
  Paper,
  Tabs,
  Tab
} from '@mui/material';
import {
  Dashboard as DashboardIcon,
  HealthAndSafety as HealthIcon,
  Article as LogIcon
} from '@mui/icons-material';
import {
  Security as SecurityIcon,
  CloudSync as OTAIcon,
  Terminal as CommandIcon,
  Compress as CompressionIcon
} from '@mui/icons-material';
import { useQuery } from '@tanstack/react-query';
import {
  getAllStats,
  getCompressionStats,
  getSecurityStats,
  getOTAStats,
  getCommandStats
} from '../api/diagnostics';
import StatisticsCard from '../components/common/StatisticsCard';
import DiagnosticsSummary from '../components/diagnostics/DiagnosticsSummary';
import LogViewer from '../components/diagnostics/LogViewer';

const Logs = () => {
  const [activeTab, setActiveTab] = useState(0);

  // Fetch all statistics
  const { data: allStatsData } = useQuery({
    queryKey: ['allStats'],
    queryFn: getAllStats,
    refetchInterval: 30000 // Refresh every 30 seconds
  });

  // Fetch compression stats
  const { data: compressionData } = useQuery({
    queryKey: ['compressionStats'],
    queryFn: getCompressionStats,
    refetchInterval: 30000
  });

  // Fetch security stats
  const { data: securityData } = useQuery({
    queryKey: ['securityStats'],
    queryFn: getSecurityStats,
    refetchInterval: 30000
  });

  // Fetch OTA stats
  const { data: otaData } = useQuery({
    queryKey: ['otaStats'],
    queryFn: getOTAStats,
    refetchInterval: 30000
  });

  // Fetch command stats
  const { data: commandData } = useQuery({
    queryKey: ['commandStats'],
    queryFn: getCommandStats,
    refetchInterval: 30000
  });

  const allStats = allStatsData?.data || {};
  const compressionStats = compressionData?.data || {};
  const securityStats = securityData?.data || {};
  const otaStats = otaData?.data || {};
  const commandStats = commandData?.data || {};

  const handleTabChange = (event, newValue) => {
    setActiveTab(newValue);
  };

  return (
    <Container maxWidth="xl" sx={{ py: 4 }}>
      {/* Page Header */}
      <Box sx={{ mb: 4 }}>
        <Typography variant="h4" component="h1" gutterBottom>
          Diagnostics & Monitoring
        </Typography>
        <Typography variant="body1" color="text.secondary">
          System health, statistics, and diagnostic information
        </Typography>
      </Box>

      {/* Tabs */}
      <Paper sx={{ mb: 3 }}>
        <Tabs 
          value={activeTab} 
          onChange={handleTabChange}
          variant="fullWidth"
          sx={{ borderBottom: 1, borderColor: 'divider' }}
        >
          <Tab 
            icon={<DashboardIcon />} 
            label="Overview"
            iconPosition="start"
          />
          <Tab 
            icon={<HealthIcon />} 
            label="System Health"
            iconPosition="start"
          />
          <Tab 
            icon={<LogIcon />} 
            label="Logs"
            iconPosition="start"
          />
        </Tabs>
      </Paper>

      {/* Tab Content */}
      <Box>
        {/* Overview Tab */}
        {activeTab === 0 && (
          <Box>
            {/* Compression Statistics */}
            <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
              Compression Statistics
            </Typography>
            <Grid container spacing={3} sx={{ mb: 4 }}>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Total Messages"
                  value={compressionStats.total_messages || 0}
                  icon={CompressionIcon}
                  color="primary"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Compressed"
                  value={compressionStats.compressed_messages || 0}
                  icon={CompressionIcon}
                  color="info"
                  subtitle={`${Math.round((compressionStats.compressed_messages || 0) / Math.max(compressionStats.total_messages || 1, 1) * 100)}% of total`}
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Avg Compression"
                  value={Math.round((compressionStats.average_compression_ratio || 0) * 100)}
                  unit="%"
                  icon={CompressionIcon}
                  color="success"
                  subtitle="Space saved"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Total Savings"
                  value={Math.round((compressionStats.total_bytes_saved || 0) / 1024)}
                  unit=" KB"
                  icon={CompressionIcon}
                  color="primary"
                />
              </Grid>
            </Grid>

            {/* Security Statistics */}
            <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
              Security Statistics
            </Typography>
            <Grid container spacing={3} sx={{ mb: 4 }}>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Verified Messages"
                  value={securityStats.verified_messages || 0}
                  icon={SecurityIcon}
                  color="success"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Failed Verifications"
                  value={securityStats.failed_verifications || 0}
                  icon={SecurityIcon}
                  color="error"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Success Rate"
                  value={Math.round((securityStats.success_rate || 0) * 100)}
                  unit="%"
                  icon={SecurityIcon}
                  color="info"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Active Keys"
                  value={securityStats.active_keys || 0}
                  icon={SecurityIcon}
                  color="primary"
                />
              </Grid>
            </Grid>

            {/* OTA Statistics */}
            <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
              Firmware Update (OTA) Statistics
            </Typography>
            <Grid container spacing={3} sx={{ mb: 4 }}>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Total Updates"
                  value={otaStats.total_updates || 0}
                  icon={OTAIcon}
                  color="primary"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Successful"
                  value={otaStats.successful_updates || 0}
                  icon={OTAIcon}
                  color="success"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Failed"
                  value={otaStats.failed_updates || 0}
                  icon={OTAIcon}
                  color="error"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Active Sessions"
                  value={otaStats.active_sessions || 0}
                  icon={OTAIcon}
                  color="warning"
                />
              </Grid>
            </Grid>

            {/* Command Statistics */}
            <Typography variant="h6" gutterBottom sx={{ mt: 3 }}>
              Command Execution Statistics
            </Typography>
            <Grid container spacing={3} sx={{ mb: 4 }}>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Total Commands"
                  value={commandStats.total_commands || 0}
                  icon={CommandIcon}
                  color="primary"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Successful"
                  value={commandStats.successful_commands || 0}
                  icon={CommandIcon}
                  color="success"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Failed"
                  value={commandStats.failed_commands || 0}
                  icon={CommandIcon}
                  color="error"
                />
              </Grid>
              <Grid item xs={12} sm={6} md={3}>
                <StatisticsCard
                  title="Pending"
                  value={commandStats.pending_commands || 0}
                  icon={CommandIcon}
                  color="warning"
                />
              </Grid>
            </Grid>
          </Box>
        )}

        {/* System Health Tab */}
        {activeTab === 1 && (
          <DiagnosticsSummary />
        )}

        {/* Logs Tab */}
        {activeTab === 2 && (
          <LogViewer />
        )}
      </Box>
    </Container>
  );
};

export default Logs;
