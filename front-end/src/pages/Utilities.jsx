/**
 * Utilities Page
 * 
 * Development and testing utilities
 * Features:
 * - Firmware preparation tool
 * - Cryptographic key generator
 * - Compression benchmarking
 * - API testing tool
 */

import React, { useState } from 'react';
import {
  Box,
  Container,
  Typography,
  Tabs,
  Tab,
  Paper
} from '@mui/material';
import {
  Build as FirmwareIcon,
  VpnKey as KeyIcon,
  Speed as BenchmarkIcon,
  BugReport as TestIcon
} from '@mui/icons-material';
import APITester from '../components/utilities/APITester';
import FirmwarePrep from '../components/utilities/FirmwarePrep';
import KeyGenerator from '../components/utilities/KeyGenerator';
import CompressionBench from '../components/utilities/CompressionBench';

const Utilities = () => {
  const [activeTab, setActiveTab] = useState(0);

  const handleTabChange = (event, newValue) => {
    setActiveTab(newValue);
  };

  return (
    <Container maxWidth="xl" sx={{ py: 4 }}>
      {/* Page Header */}
      <Box sx={{ mb: 4 }}>
        <Typography variant="h4" component="h1" gutterBottom>
          Development Utilities
        </Typography>
        <Typography variant="body1" color="text.secondary">
          Tools for firmware preparation, key generation, and performance testing
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
            icon={<FirmwareIcon />}
            label="Firmware Prep"
            iconPosition="start"
          />
          <Tab
            icon={<KeyIcon />}
            label="Key Generator"
            iconPosition="start"
          />
          <Tab
            icon={<BenchmarkIcon />}
            label="Compression Bench"
            iconPosition="start"
          />
          <Tab
            icon={<TestIcon />}
            label="API Tester"
            iconPosition="start"
          />
        </Tabs>
      </Paper>

      {/* Tab Content */}
      <Box>
        {activeTab === 0 && <FirmwarePrep />}
        {activeTab === 1 && <KeyGenerator />}
        {activeTab === 2 && <CompressionBench />}
        {activeTab === 3 && <APITester />}
      </Box>
    </Container>
  );
};

export default Utilities;
