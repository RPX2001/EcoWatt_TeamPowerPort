#!/bin/bash

# EcoWatt Frontend Phase 1 Completion Test Script

echo "================================================"
echo "EcoWatt Frontend - Phase 1 Completion Test"
echo "================================================"
echo ""

# Colors
GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

cd "$(dirname "$0")"

echo "📦 1. Checking dependencies..."
if [ -d "node_modules" ]; then
    echo -e "${GREEN}✅ node_modules exists${NC}"
else
    echo -e "${RED}❌ node_modules not found. Run: npm install${NC}"
    exit 1
fi

echo ""
echo "📁 2. Checking file structure..."

files=(
    "src/components/common/Navbar.jsx"
    "src/components/common/Sidebar.jsx"
    "src/components/common/Footer.jsx"
    "src/components/dashboard/DeviceSelector.jsx"
    "src/components/dashboard/MetricsCard.jsx"
    "src/components/dashboard/TimeSeriesChart.jsx"
    "src/components/utilities/APITester.jsx"
    "src/pages/Dashboard.jsx"
    "src/pages/Configuration.jsx"
    "src/pages/Commands.jsx"
    "src/pages/FOTA.jsx"
    "src/pages/Logs.jsx"
    "src/pages/Utilities.jsx"
    "src/pages/Testing.jsx"
    "src/api/axios.js"
    "src/api/devices.js"
    "src/api/aggregation.js"
    "src/api/websocket.js"
    "src/theme/theme.js"
)

all_files_exist=true
for file in "${files[@]}"; do
    if [ -f "$file" ]; then
        echo -e "${GREEN}✅${NC} $file"
    else
        echo -e "${RED}❌${NC} $file ${RED}(missing)${NC}"
        all_files_exist=false
    fi
done

echo ""
echo "🔍 3. Checking Flask backend..."
if curl -s http://localhost:5001/health > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Flask backend is running on port 5001${NC}"
else
    echo -e "${YELLOW}⚠️  Flask backend not running on port 5001${NC}"
    echo -e "${YELLOW}   Start it with: cd ../flask && python flask_server_modular.py${NC}"
fi

echo ""
echo "🧪 4. Running build test..."
if npm run build > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Build successful${NC}"
else
    echo -e "${RED}❌ Build failed${NC}"
    all_files_exist=false
fi

echo ""
echo "================================================"
if [ "$all_files_exist" = true ]; then
    echo -e "${GREEN}✅ Phase 1 Complete! All components created.${NC}"
    echo ""
    echo "Next steps:"
    echo "  1. Start Flask: cd ../flask && python flask_server_modular.py"
    echo "  2. Start Frontend: npm run dev"
    echo "  3. Open browser: http://localhost:5173"
    echo "  4. Test API connection in Utilities page"
else
    echo -e "${RED}❌ Some files are missing. Please review above.${NC}"
fi
echo "================================================"
