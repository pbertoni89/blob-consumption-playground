#!/user/bin/env python3

import pandas
import os
import re
import numpy
from matplotlib import pyplot as plt


def df_scatter(df, xcol='act_time_s', ycol='exp_time_s', cat_col='elf'):
    fig, ax = plt.subplots()
    categories = numpy.unique(df[cat_col])
    colors = numpy.linspace(0, 1, len(categories))
    colordict = dict(zip(categories, colors))

    df['Color'] = df[cat_col].apply(lambda x: colordict[x])
    ax.scatter(df[xcol], df[ycol], c=df.Color)
    return fig


def main(results_dir: str, do_show: bool = False):
    re_cmd = re.compile(r'--elf (\w+) --inms (\d+) --outms (\d+) --njobs (\d+) --debug \d --nblobs (\d+)')
    re_alpha = re.compile(r'.*alpha.*-> (\d+).*-> (\d+).*\[s] (\d+)')
    re_omega = re.compile(r'.*omega.*\[ms] (\d+).*')
    dd = {'elf': [], 'in_ms': [], 'out_ms': [], 'n_jobs': [], 'n_blobs': [], 'exp_rate_i': [], 'exp_rate_o': [],
          'exp_time_s': [], 'act_time_s': []}
    df = pandas.DataFrame(dd)
    print(df)

    fn_inp, fn_out = [os.path.join(results_dir, f) for f in ['results.txt', 'analysis.']]
    with open(fn_inp, 'r') as fo:
        elf, in_ms, out_ms, n_jobs, n_blobs, exp_rate_i, exp_rate_o, exp_time_s, act_time_s = (None for _ in range(9))
        for line in fo.readlines():
            ln = line.strip()
            m_cmd = re_cmd.match(ln)
            if m_cmd:
                elf, in_ms, out_ms, n_jobs, n_blobs = m_cmd.groups()
                # print(f'C {m_cmd.groups()}')
            else:
                m_alpha = re_alpha.match(ln)
                if m_alpha:
                    exp_rate_i, exp_rate_o, exp_time_s = m_alpha.groups()
                    # print(f'A {m_alpha.groups()}')
                else:
                    m_omega = re_omega.match(ln)
                    if m_omega:
                        act_time_s, = m_omega.groups()
                        # print(f'O {m_omega.groups()}')
                        ls_row = [elf, in_ms, out_ms, n_jobs, n_blobs, exp_rate_i, exp_rate_o, exp_time_s, act_time_s]
                        row = pandas.Series(ls_row, index=df.columns)
                        # print(f'appending {row}')
                        df = df.append(row, ignore_index=True)
    # print(df.to_string())

    # - - - DATAFRAME POSTPROCESSING
    dtypes = {d: int for d in ['in_ms', 'out_ms', 'n_jobs', 'n_blobs', 'exp_rate_i',
                               'exp_rate_o', 'exp_time_s', 'act_time_s']}
    dtypes['elf'] = 'category'
    df = df.astype(dtypes)
    df['act_time_s'] /= 1000  # they are milliseconds

    # Strange copy behaviour I don't have so much time... https://stackoverflow.com/questions/23296282
    df_p = df.loc[df['elf'] == 'pool']
    df_a = df.loc[df['elf'] == 'async']

    n_jobs_max = df['n_jobs'].max()  # shall be equal to NUM_CORES

    def check_async_multicore(fixed: bool):
        df_async = df.loc[df['elf'] == 'async']
        df_async_multicore = df.loc[(df['n_jobs'] > 1) & (df['elf'] == 'async')]
        nasync_multicore = len(df_async_multicore)
        nasync = len(df_async)
        if nasync_multicore > 0 and not fixed:
            raise ValueError(f'async was marked 1 n_jobs, not {n_jobs_max}: {nasync_multicore}')
        elif nasync_multicore != nasync and fixed:
            raise ValueError(f'async is marked {n_jobs_max} n_jobs, not {nasync}: {nasync_multicore}')

    check_async_multicore(False)

    df.loc[df['elf'] == 'async', 'n_jobs'] = n_jobs_max

    # FIXME DUNNO WHY BUT IT WORKS AND it even shows us why `async` could be better
    magic = True
    # in facts, exp_time_s estimation (done in C++) is wrong for `async` elf, because it internally
    # pretends to work on a single job, therefore messing up with `pool` estimation. However, act_time_s clearly
    # tells us that `async` is always on time (at least with these parameters choice, hardware included).
    # the fun fact here is that n_blobs = 1000 and this brings an identity between [ms] and [s] times
    if magic:
        df.loc[df['elf'] == 'async', 'exp_time_s'] = \
            df.loc[df['elf'] == 'async', 'in_ms'] * (df.loc[df['elf'] == 'async', 'n_blobs'] / 1000)

    check_async_multicore(True)

    df['extra_time_s'] = (df['act_time_s'] - df['exp_time_s']).astype(int)

    df = df.astype(dtypes)  # re-cast after any edit and before dumping
    print(df.to_string())
    df.to_csv(fn_out + 'csv')

    # - - - DATAFRAME PLOT
    # df.plot(y='act_time_s')
    # plt.plot(df['act_time_s'])
    f, ax = plt.subplots(1)
    ax.scatter(x=df_p['n_jobs'], y=df_p['exp_time_s'], c='black')
    ax.scatter(x=df_a['n_jobs'], y=df_a['exp_time_s'], c='red')  # , s=df['act_time_s'])
    plt.xlabel('n_jobs')
    plt.ylabel('exp_time_s')
    f, ax = plt.subplots(1)
    ax.scatter(x=df['act_time_s'], y=df['exp_time_s'])
    plt.xlabel('act_time_s')
    plt.ylabel('exp_time_s')
    _ = df_scatter(df, xcol='act_time_s', ycol='exp_time_s', cat_col='elf')
    plt.xlabel('act_time_s')
    plt.ylabel('exp_time_s')
    _ = df_scatter(df, xcol='n_jobs', ycol='extra_time_s', cat_col='elf')
    plt.xlabel('n_jobs')
    plt.ylabel('extra_time_s')
    plt.savefig(fn_out + 'png')
    if do_show:
        plt.show()


if __name__ == '__main__':
    hn = 'stampede'
    # hn = 'ammiraglia'
    path_to_dir = os.path.join(os.path.dirname(__file__), f'data-{hn}')
    main(path_to_dir, do_show=True)
