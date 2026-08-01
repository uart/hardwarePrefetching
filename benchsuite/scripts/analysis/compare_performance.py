#!/usr/bin/env python3
"""
Performance Comparison Analysis for Benchmarks
Extracts performance data from benchmark runs and provides comprehensive comparison analysis.
Automatically processes all available runs and compares against configured baseline.
"""
import re
import pandas as pd
import argparse
from pathlib import Path
import sys
from datetime import datetime

def load_config():
    """Load configuration from benchsuite.conf"""
    config_file = Path(__file__).parent.parent.parent / 'config' / 'benchsuite.conf'
    config = {}
    
    if config_file.exists():
        with open(config_file, 'r') as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith('#') and '=' in line:
                    key, value = line.split('=', 1)
                    config[key.strip()] = value.strip()
    
    return config

def setup_paths():
    """Setup paths based on project structure."""
    config = load_config()
    
    # Use config value or fall back to relative path from project root
    if 'RESULTS_DIR' in config and config['RESULTS_DIR']:
        # Expand tilde and environment variables in the path
        import os
        results_path = os.path.expanduser(os.path.expandvars(config['RESULTS_DIR']))
        return results_path
    else:
        # Fallback to relative path from project root
        project_root = Path(__file__).parent.parent.parent
        return str(project_root / 'results')

def parse_time_output(content):
    """Parse /usr/bin/time -v output for performance metrics."""
    metrics = {}
    
    # Time parsing patterns
    time_patterns = [
        r'Elapsed \(wall clock\) time \(h:mm:ss or m:ss\): (\d+):(\d+\.\d+)',
        r'Elapsed \(wall clock\) time \(h:mm:ss or m:ss\): (\d+):(\d+):(\d+\.\d+)',
        r'Elapsed \(wall clock\) time \(h:mm:ss or m:ss\): (\d+\.\d+)'
    ]
    
    # Try to parse elapsed time
    elapsed_seconds = 0
    for pattern in time_patterns:
        match = re.search(pattern, content)
        if match:
            if len(match.groups()) == 2:  # m:ss.ss format
                minutes, seconds = match.groups()
                elapsed_seconds = int(minutes) * 60 + float(seconds)
            elif len(match.groups()) == 3:  # h:mm:ss.ss format
                hours, minutes, seconds = match.groups()
                elapsed_seconds = int(hours) * 3600 + int(minutes) * 60 + float(seconds)
            else:  # ss.ss format
                elapsed_seconds = float(match.group(1))
            break
    
    metrics['elapsed_seconds'] = elapsed_seconds
    
    # Parse other metrics
    patterns = {
        'user_time': r'User time \(seconds\): ([\d.]+)',
        'system_time': r'System time \(seconds\): ([\d.]+)',
        'cpu_percent': r'Percent of CPU this job got: (\d+)%',
        'max_memory_kb': r'Maximum resident set size \(kbytes\): (\d+)',
        'major_page_faults': r'Major \(requiring I/O\) page faults: (\d+)',
        'minor_page_faults': r'Minor \(reclaiming a frame\) page faults: (\d+)',
        'voluntary_context_switches': r'Voluntary context switches: (\d+)',
        'involuntary_context_switches': r'Involuntary context switches: (\d+)',
        'file_system_inputs': r'File system inputs: (\d+)',
        'file_system_outputs': r'File system outputs: (\d+)'
    }
    
    for metric, pattern in patterns.items():
        match = re.search(pattern, content)
        if match:
            value = match.group(1)
            metrics[metric] = float(value) if '.' in value else int(value)
        else:
            metrics[metric] = 0
    
    # Convert memory to MB
    metrics['max_memory_mb'] = metrics.get('max_memory_kb', 0) / 1024
    
    return metrics

def extract_run_data(results_dir, run_type='all'):
    """Extract performance data from benchmark runs."""
    results_path = Path(results_dir) / 'reports'
    all_data = []
    
    # Determine which directories to process
    if run_type == 'baseline':
        # For baseline, look in results/reports directory for timestamp dirs
        search_dirs = [results_path]
    elif run_type == 'dpf':
        search_dirs = [results_path]
    else:  # all
        search_dirs = [results_path]
    
    for search_dir in search_dirs:
        if not search_dir.exists():
            print(f"Warning: Directory {search_dir} does not exist")
            continue
            
        print(f"Scanning for results in: {search_dir}")
        
        # Find timestamp directories
        timestamp_dirs = [d for d in search_dir.iterdir() if d.is_dir() and re.match(r'\d{8}-\d{6}', d.name)]
        print(f"Found {len(timestamp_dirs)} timestamp directories")
        
        # For 'current' type, only process the most recent directory to get just this run's data
        if run_type == 'current':
            timestamp_dirs = sorted(timestamp_dirs, key=lambda x: x.name)[-1:] if timestamp_dirs else []
            print(f"Processing only most recent run for current type: {len(timestamp_dirs)} directory")

        for timestamp_dir in timestamp_dirs:
            print(f"\nProcessing: {timestamp_dir.name}")

            # Check if this is baseline or DPF based on timestamp directory name
            # Baseline runs: _quick, _full, _baseline (any RUN_MODE with BASELINE_MODE=true)
            # DPF runs: _dpf (BASELINE_MODE=false)
            is_baseline = not timestamp_dir.name.endswith('_dpf')

            # Skip based on run_type filter
            if run_type == 'baseline' and not is_baseline:
                continue
            elif run_type == 'dpf' and is_baseline:
                continue
            elif run_type == 'current':
                # Current includes all recent runs regardless of type
                pass
            
            benchmark_speed_dir = timestamp_dir / 'benchmark_speed'
            if not benchmark_speed_dir.exists():
                print(f"  No benchmark_speed directory found in {timestamp_dir.name}")
                continue
            
            # Process each benchmark
            for benchmark_dir in benchmark_speed_dir.iterdir():
                if not benchmark_dir.is_dir():
                    continue
                    
                benchmark = benchmark_dir.name
                print(f"  Processing benchmark: {benchmark}")
                
                # Find run directories - look for both 'run_' and 'ref' directories
                run_dirs = [d for d in benchmark_dir.iterdir() if d.is_dir() and (d.name.startswith('run_') or d.name == 'ref')]
                
                if not run_dirs:
                    print(f"    No run directories found for {benchmark}")
                    continue
                
                successful_runs = []
                for run_dir in run_dirs:
                    # Look for either speccmds.stdout (standard SPEC) or our custom log files
                    stdout_file = run_dir / 'speccmds.stdout'
                    log_files = list(run_dir.glob('*.log.core*'))
                    
                    if stdout_file.exists():
                        # Standard SPEC output
                        try:
                            with open(stdout_file, 'r') as f:
                                content = f.read()
                            
                            # Check if this contains time output
                            if 'Elapsed (wall clock) time' not in content:
                                continue
                            
                            metrics = parse_time_output(content)
                            if metrics['elapsed_seconds'] > 0:
                                # Extract core info from run directory name
                                core_match = re.search(r'run_base_speed_.*\.(\d+)', run_dir.name)
                                core = int(core_match.group(1)) if core_match else 0
                                
                                run_data = {
                                    'timestamp': timestamp_dir.name,
                                    'benchmark': benchmark,
                                    'run_id': run_dir.name,
                                    'core': core,
                                    'run_type': 'baseline' if is_baseline else 'dpf',
                                    **metrics
                                }
                                
                                all_data.append(run_data)
                                successful_runs.append(metrics['elapsed_seconds'])
                        
                        except Exception as e:
                            # Skip files that can't be processed
                            continue
                            
                    elif log_files:
                        # Our custom log files
                        try:
                            for log_file in log_files:
                                with open(log_file, 'r') as f:
                                    content = f.read()
                                
                                # Check if this contains time output
                                if 'Elapsed (wall clock) time' not in content:
                                    continue
                                
                                metrics = parse_time_output(content)
                                if metrics['elapsed_seconds'] > 0:
                                    # Extract core info from log file name
                                    core_match = re.search(r'\.core(\d+)', log_file.name)
                                    core = int(core_match.group(1)) if core_match else 0
                                    
                                    run_data = {
                                        'timestamp': timestamp_dir.name,
                                        'benchmark': benchmark,
                                        'run_id': log_file.name,
                                        'core': core,
                                        'run_type': 'baseline' if is_baseline else 'dpf',
                                        **metrics
                                    }
                                    
                                    all_data.append(run_data)
                                    successful_runs.append(metrics['elapsed_seconds'])
                        
                        except Exception as e:
                            # log and skip files that can't be processed
                            print(f"    Error processing {log_file.name}: {e}")
                            continue
                
                if successful_runs:
                    print(f"    Found {len(successful_runs)} successful runs")
                    for i, time_val in enumerate(successful_runs):  # Show all runs
                        print(f"      Run {i+1}: {time_val:.2f}s elapsed")
                else:
                    print(f"    No successful runs found for {benchmark}")
    
    return pd.DataFrame(all_data)

def aggregate_data(detailed_df):
    """Create aggregated statistics from detailed data."""
    if detailed_df.empty:
        return pd.DataFrame()
    
    # Group by benchmark and run_type
    metrics_to_aggregate = [
        'elapsed_seconds', 'user_time', 'system_time', 'cpu_percent',
        'max_memory_mb', 'major_page_faults', 'minor_page_faults',
        'voluntary_context_switches', 'involuntary_context_switches',
        'file_system_inputs', 'file_system_outputs'
    ]
    
    aggregated_data = []
    
    for (benchmark, run_type), group in detailed_df.groupby(['benchmark', 'run_type']):
        agg_row = {'benchmark': benchmark, 'run_type': run_type}
        
        for metric in metrics_to_aggregate:
            if metric in group.columns:
                values = group[metric].dropna()
                if len(values) > 0:
                    agg_row[f'{metric}_mean'] = values.mean()
                    agg_row[f'{metric}_std'] = values.std()
                    agg_row[f'{metric}_min'] = values.min()
                    agg_row[f'{metric}_max'] = values.max()
                    agg_row[f'{metric}_count'] = len(values)
        
        aggregated_data.append(agg_row)
    
    return pd.DataFrame(aggregated_data)

def find_all_runs(reports_dir):
    """Find all available benchmark runs"""
    runs = []
    reports_path = Path(reports_dir)
    
    if not reports_path.exists():
        print(f"Reports directory not found: {reports_path}")
        return runs
    
    for item in reports_path.iterdir():
        if item.is_dir() and re.match(r'^20\d{6}-\d{6}', item.name):
            # Check if it has benchmark data
            if (item / 'benchmark_speed').exists():
                runs.append(item.name)
    
    return sorted(runs)

def extract_core_id(filename):
    """Extract core ID from log filename"""
    match = re.search(r'\.core(\d+)$', filename)
    return int(match.group(1)) if match else 0

def parse_time_string(time_str):
    """Convert time string (h:mm:ss or m:ss) to seconds"""
    parts = time_str.split(':')
    if len(parts) == 2:  # m:ss
        return int(parts[0]) * 60 + float(parts[1])
    elif len(parts) == 3:  # h:mm:ss
        return int(parts[0]) * 3600 + int(parts[1]) * 60 + float(parts[2])
    else:
        return float(time_str)  # Assume already in seconds

def extract_single_run_data(run_dir):
    """Extract performance data from a single run directory (simplified version for comparison)"""
    data = []
    benchmark_speed_dir = Path(run_dir) / 'benchmark_speed'
    
    if not benchmark_speed_dir.exists():
        return pd.DataFrame()
    
    for benchmark_dir in benchmark_speed_dir.iterdir():
        if not benchmark_dir.is_dir():
            continue
            
        ref_dir = benchmark_dir / 'ref'
        if not ref_dir.exists():
            continue
        
        # Extract benchmark name from directory (e.g., "600.perlbench" -> "perlbench")
        benchmark_match = re.match(r'(\d+)\.(.+)', benchmark_dir.name)
        if not benchmark_match:
            continue
        
        benchmark_name = benchmark_match.group(2)
        
        # Find log files and extract metrics
        for log_file in ref_dir.glob('*.log.core*'):
            try:
                with open(log_file, 'r') as f:
                    content = f.read()
                
                # Extract time metrics
                elapsed_match = re.search(r'Elapsed \(wall clock\) time \(h:mm:ss or m:ss\): (.+)', content)
                max_memory_match = re.search(r'Maximum resident set size \(kbytes\): (.+)', content)
                
                if elapsed_match:
                    elapsed_str = elapsed_match.group(1)
                    elapsed_seconds = parse_time_string(elapsed_str)
                    
                    core_id = extract_core_id(log_file.name)
                    
                    run_data = {
                        'run_id': Path(run_dir).name,
                        'benchmark': benchmark_name,
                        'core_id': core_id,
                        'elapsed_seconds': elapsed_seconds,
                        'max_memory_mb': float(max_memory_match.group(1)) / 1024 if max_memory_match else None,
                    }
                    data.append(run_data)
                    
            except Exception as e:
                continue
    
    return pd.DataFrame(data)

def aggregate_run_data(df):
    """Aggregate data by run and benchmark (average across cores)"""
    if df.empty:
        return df
    
    agg_df = df.groupby(['run_id', 'benchmark']).agg({
        'elapsed_seconds': 'mean',
        'max_memory_mb': 'mean'
    }).reset_index()
    
    return agg_df

def compare_runs_to_baseline(all_runs_data, baseline_data):
    """Compare all runs against baseline data"""
    comparison_results = []
    
    # Get unique benchmarks
    benchmarks = all_runs_data['benchmark'].unique()
    
    for benchmark in benchmarks:
        # Get baseline data for this benchmark
        baseline_bench = baseline_data[baseline_data['benchmark'] == benchmark]
        if baseline_bench.empty:
            continue
        
        baseline_time = baseline_bench['elapsed_seconds'].iloc[0]
        baseline_memory = baseline_bench['max_memory_mb'].iloc[0]
        
        # Compare each run for this benchmark
        bench_runs = all_runs_data[all_runs_data['benchmark'] == benchmark]
        
        for _, run_row in bench_runs.iterrows():
            result = {
                'benchmark': benchmark,
                'run_id': run_row['run_id'],
                'baseline_time_sec': baseline_time,
                'run_time_sec': run_row['elapsed_seconds'],
                'time_diff_sec': run_row['elapsed_seconds'] - baseline_time,
                'time_change_pct': ((run_row['elapsed_seconds'] - baseline_time) / baseline_time * 100) if baseline_time > 0 else 0,
                'speedup': baseline_time / run_row['elapsed_seconds'] if run_row['elapsed_seconds'] > 0 else 0,
                'baseline_memory_mb': baseline_memory,
                'run_memory_mb': run_row['max_memory_mb'],
                'memory_diff_mb': (run_row['max_memory_mb'] - baseline_memory) if pd.notna(run_row['max_memory_mb']) and pd.notna(baseline_memory) else None,
                'memory_change_pct': ((run_row['max_memory_mb'] - baseline_memory) / baseline_memory * 100) if pd.notna(run_row['max_memory_mb']) and pd.notna(baseline_memory) and baseline_memory > 0 else None
            }
            comparison_results.append(result)
    
    return pd.DataFrame(comparison_results)

def print_summary_table(comparison_df, baseline_id):
    """Print a comprehensive summary table to console without emojis"""
    print(f"\n{'='*80}")
    print(f"PERFORMANCE COMPARISON vs BASELINE: {baseline_id}")
    print(f"{'='*80}")
    
    if comparison_df.empty:
        print("No comparison data available")
        return
    
    # Group by benchmark and show all runs
    benchmarks = sorted(comparison_df['benchmark'].unique())
    
    for benchmark in benchmarks:
        bench_data = comparison_df[comparison_df['benchmark'] == benchmark].copy()
        bench_data = bench_data.sort_values('run_id')
        
        print(f"\n{benchmark.upper()}")
        print(f"{'-'*60}")
        
        for _, row in bench_data.iterrows():
            memory_str = f"Memory: {row['run_memory_mb']:.1f}MB"
            if pd.notna(row['memory_change_pct']):
                memory_str += f" ({row['memory_change_pct']:+.1f}%)"
            
            baseline_indicator = " (baseline)" if row['run_id'] == baseline_id else ""
            
            print(f"{row['run_id']:<25}{baseline_indicator} | "
                  f"Time: {row['run_time_sec']:.3f}s ({row['time_change_pct']:+.1f}%) | "
                  f"Speedup: {row['speedup']:.3f}x | {memory_str}")
    
    # Overall summary
    print(f"\n{'='*60}")
    print(f"OVERALL SUMMARY")
    print(f"{'='*60}")
    
    total_runs = len(comparison_df['run_id'].unique())
    avg_speedup = comparison_df['speedup'].mean()
    best_speedup = comparison_df['speedup'].max()
    worst_speedup = comparison_df['speedup'].min()
    
    print(f"Total runs analyzed: {total_runs}")
    print(f"Average speedup: {avg_speedup:.3f}x")
    print(f"Best speedup: {best_speedup:.3f}x")
    print(f"Worst speedup: {worst_speedup:.3f}x")
    
    # Performance trend
    improvements = len(comparison_df[comparison_df['speedup'] > 1.0])
    regressions = len(comparison_df[comparison_df['speedup'] < 1.0])
    
    print(f"Performance improvements: {improvements} ({improvements/len(comparison_df)*100:.1f}%)")
    print(f"Performance regressions: {regressions} ({regressions/len(comparison_df)*100:.1f}%)")

def save_comparison_csv(comparison_df, output_file):
    """Save comparison results to CSV"""
    output_path = Path(output_file)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    
    # Round numerical columns for cleaner output
    numeric_cols = ['baseline_time_sec', 'run_time_sec', 'time_diff_sec', 'time_change_pct', 
                   'speedup', 'baseline_memory_mb', 'run_memory_mb', 'memory_diff_mb', 'memory_change_pct']
    
    for col in numeric_cols:
        if col in comparison_df.columns:
            comparison_df[col] = comparison_df[col].round(3)
    
    comparison_df.to_csv(output_path, index=False)
    print(f"\nDetailed comparison saved to: {output_path}")

def run_comparison_analysis(results_dir, config):
    """Run comprehensive comparison analysis of all available runs"""
    baseline_id = config.get('REFERENCE_BASELINE', '')
    reports_dir = Path(results_dir) / 'reports'
    
    # Find all available runs
    all_runs = find_all_runs(reports_dir)
    if len(all_runs) < 1:
        print("No benchmark runs found for comparison analysis.")
        return
    
    print(f"\nFound {len(all_runs)} benchmark runs")
    
    # Determine baseline to use
    if not baseline_id:
        # No baseline configured - use first run chronologically as baseline
        baseline_id = sorted(all_runs)[0]  # First by timestamp
        print(f"No baseline configured in config file.")
        print(f"Using first run as baseline: {baseline_id}")
        print("Tip: Use set_baseline_reference.sh to configure a specific baseline for future comparisons.")
    else:
        print(f"Using configured baseline: {baseline_id}")
        # Verify baseline exists
        if baseline_id not in all_runs:
            print(f"ERROR: Configured baseline run '{baseline_id}' not found in available runs")
            print(f"Available runs: {', '.join(all_runs)}")
            # Fall back to first run
            baseline_id = sorted(all_runs)[0]
            print(f"Falling back to first run as baseline: {baseline_id}")
    
    if len(all_runs) < 2:
        print(f"Only one run found ({baseline_id}). No comparison possible.")
        print("Run more benchmarks to enable comparison analysis.")
        return
    
    # Extract data from all runs
    all_data = []
    for run_id in all_runs:
        print(f"Processing run: {run_id}")
        run_dir = reports_dir / run_id
        run_data = extract_single_run_data(run_dir)
        if not run_data.empty:
            all_data.append(run_data)
    
    if not all_data:
        print("No valid benchmark data found for comparison")
        return
    
    # Combine all data
    combined_df = pd.concat(all_data, ignore_index=True)
    aggregated_df = aggregate_run_data(combined_df)
    
    # Separate baseline data
    baseline_data = aggregated_df[aggregated_df['run_id'] == baseline_id]
    if baseline_data.empty:
        print(f"No data found for baseline: {baseline_id}")
        return
    
    # Compare all runs to baseline
    comparison_df = compare_runs_to_baseline(aggregated_df, baseline_data)
    
    # Print summary table
    print_summary_table(comparison_df, baseline_id)
    
    # Save to CSV
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    output_file = Path(results_dir) / 'csv' / f'performance_comparison_{timestamp}.csv'
    save_comparison_csv(comparison_df, output_file)

def main():
    """Main performance comparison and analysis function."""
    parser = argparse.ArgumentParser(description='Compare benchmark performance across runs with comprehensive analysis')
    parser.add_argument('--type', choices=['all', 'baseline', 'dpf', 'current'], default='all',
                        help='Type of data to extract (default: all)')  
    parser.add_argument('--output-dir', default='data',
                        help='Output directory for CSV files (default: data)')
    parser.add_argument('--no-comparison', action='store_true',
                        help='Skip comprehensive comparison analysis')
    
    args = parser.parse_args()
    
    # Load configuration and setup paths
    config = load_config()
    results_dir = setup_paths()
    script_dir = Path(__file__).parent
    project_root = script_dir.parent.parent
    output_dir = project_root / args.output_dir
    
    print("Performance Comparison and Analysis")
    print("=" * 60)
    print(f"Results directory: {results_dir}")
    print(f"Output directory: {output_dir}")
    print(f"Extraction type: {args.type}")
    
    # Extract data
    detailed_df = extract_run_data(results_dir, args.type)
    
    if detailed_df.empty:
        print("No data found!")
        sys.exit(1)
    
    print(f"\n" + "=" * 60)
    print("EXTRACTION SUMMARY")
    print("=" * 60)
    print(f"Total runs extracted: {len(detailed_df)}")
    
    # Show breakdown by run type and benchmark
    if 'run_type' in detailed_df.columns:
        print("\nBreakdown by run type:")
        for run_type, group in detailed_df.groupby('run_type'):
            print(f"  {run_type}: {len(group)} runs")
    
    print("\nBreakdown by benchmark:")
    for benchmark, group in detailed_df.groupby('benchmark'):
        print(f"  {benchmark}: {len(group)} runs")
    
    print(f"Data extraction complete - {len(detailed_df)} runs processed")
    
    print("\nExtraction complete!")
    
    # Run comprehensive comparison analysis (unless disabled)
    if not args.no_comparison:
        run_comparison_analysis(results_dir, config)

if __name__ == "__main__":
    main()
