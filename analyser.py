#!/user/bin/env python3
import typing

import pandas
import os
import re
import numpy
from matplotlib import pyplot as plt


def _scatter_color(df: pandas.DataFrame, xcol: str, ycol: str, cat_col: typing.Optional[str] = None, ax_curr=None,
                   title=None) -> None:

    if cat_col is None:
        c = cat_col
    else:
        categories = numpy.unique(df[cat_col])
        colors = numpy.linspace(0, 1, len(categories))
        colordict = dict(zip(categories, colors))
        df['Color'] = df[cat_col].apply(lambda x: colordict[x])
        c = df.Color
    ax_curr.scatter(df[xcol], df[ycol], c=c)
    plt.xlabel(xcol)
    plt.ylabel(ycol)
    if title is not None:
        plt.title(title)


def _parse_results(results_dir: str) -> pandas.DataFrame:
    re_cmd = re.compile(r'.*--elf (\w+) --inms (\d+) --outms (\d+) --njobs (\d+) --debug \d --nblobs (\d+)')
    re_alpha = re.compile(r'.*alpha.*-> (\d+).*-> (\d+).*\[s] (\d+)')
    re_task_miss = re.compile(r'.*MaxTaskEver (\d+).*MaxMissEver (\d+)')
    re_omega = re.compile(r'.*omega.*elapsed \[s] (\d+).*')
    dd = {'i_elf': [], 'i_in_ms': [], 'i_out_ms': [], 'i_n_jobs': [], 'i_n_blobs': [],
          'o_max_task': [], 'o_max_miss': [],
          'o_in_exp_rate': [], 'o_out_exp_rate': [], 'o_exp_time_s': [], 'o_act_time_s': []}
    df = pandas.DataFrame(dd)

    with open(os.path.join(results_dir, 'results.txt'), 'r') as fo:
        elf, inms, outms, njobs, nblobs, mtask, mmiss, exp_rate_i, exp_rate_o, exp_time_s, act_time_s = \
            (None for _ in range(11))
        for line in fo.readlines():
            ln = line.strip()
            m_cmd = re_cmd.match(ln)
            if m_cmd:
                elf, inms, outms, njobs, nblobs = m_cmd.groups()
                # print(f'C {m_cmd.groups()}')
                continue
            m_alpha = re_alpha.match(ln)
            if m_alpha:
                exp_rate_i, exp_rate_o, exp_time_s = m_alpha.groups()
                # print(f'A {m_alpha.groups()}')
                continue
            m_task_miss = re_task_miss.match(ln)
            if m_task_miss:
                mtask, mmiss = m_task_miss.groups()
                continue
            m_omega = re_omega.match(ln)
            if m_omega:
                act_time_s, = m_omega.groups()
                # print(f'O {m_omega.groups()}')
                ls_row = [elf, inms, outms, njobs, nblobs, mtask, mmiss, exp_rate_i, exp_rate_o, exp_time_s, act_time_s]
                row = pandas.Series(ls_row, index=df.columns)
                # print(f'appending {row}')
                df = df.append(row, ignore_index=True)
                continue
    return df


def _postprocessing(df: pandas.DataFrame) -> pandas.DataFrame:
    dtypes = {d: int for d in ['i_in_ms', 'i_out_ms', 'i_n_jobs', 'i_n_blobs', 'o_max_task', 'o_max_miss',
                               'o_in_exp_rate', 'o_out_exp_rate', 'o_exp_time_s', 'o_act_time_s']}
    dtypes['i_elf'] = 'category'
    df = df.astype(dtypes)

    n_jobs_max = df['i_n_jobs'].max()  # shall be equal to NUM_CORES

    # Strange copy behaviour I don't have so much time... https://stackoverflow.com/questions/23296282
    # df_p = df.loc[df['i_elf'] == 'pool']
    # df_a = df.loc[df['i_elf'] == 'async']

    def check_async_multicore(fixed: bool):
        df_async = df.loc[df['i_elf'] == 'async']
        df_async_multicore = df.loc[(df['i_n_jobs'] > 1) & (df['i_elf'] == 'async')]
        nasync_multicore = len(df_async_multicore)
        nasync = len(df_async)
        if nasync_multicore > 0 and not fixed:
            raise ValueError(f'async was marked 1 n_jobs, not {n_jobs_max}: {nasync_multicore}')
        elif nasync_multicore != nasync and fixed:
            raise ValueError(f'async is marked {n_jobs_max} n_jobs, not {nasync}: {nasync_multicore}')

    check_async_multicore(False)

    df.loc[df['i_elf'] == 'async', 'i_n_jobs'] = n_jobs_max

    # FIXME DUNNO WHY BUT IT WORKS AND it even shows us why `async` could be better
    magic = True
    # in facts, exp_time_s estimation (done in C++) is wrong for `async` elf, because it internally
    # pretends to work on a single job, therefore messing up with `pool` estimation. However, act_time_s clearly
    # tells us that `async` is always on time (at least with these parameters choice, hardware included).
    # the fun fact here is that n_blobs = 1000 and this brings an identity between [ms] and [s] times
    if magic:
        df.loc[df['i_elf'] == 'async', 'o_exp_time_s'] = \
            df.loc[df['i_elf'] == 'async', 'i_in_ms'] * (df.loc[df['i_elf'] == 'async', 'i_n_blobs'] / 1000)

    check_async_multicore(True)

    df['p_extra_time_s'] = (df['o_act_time_s'] - df['o_exp_time_s']).astype(int)

    df['p_load'] = (df['i_out_ms'] / df['i_n_jobs']) / df['i_in_ms']

    df = df.astype(dtypes)  # re-cast after any edit and before dumping
    return df


def _plot(results_dir: str, df: pandas.DataFrame) -> None:

    fn_out = os.path.join(results_dir, 'analysis')
    print(df.to_string())
    df.to_csv(fn_out + '.csv')

    fig, ax = plt.subplots(nrows=2, ncols=2)
    fig.suptitle(os.path.split(results_dir)[-1])
    ax_curr = plt.subplot(2, 2, 1)
    _scatter_color(df, xcol='p_extra_time_s', ycol='p_load', cat_col='i_elf', ax_curr=ax_curr, title='')
    ax_curr = plt.subplot(2, 2, 2)
    _scatter_color(df, xcol='o_act_time_s', ycol='o_exp_time_s', cat_col='i_elf', ax_curr=ax_curr)
    ax_curr = plt.subplot(2, 2, 3)
    _scatter_color(df, xcol='i_n_jobs', ycol='p_extra_time_s', cat_col='i_elf', ax_curr=ax_curr)
    ax_curr = plt.subplot(2, 2, 4)
    _scatter_color(df, xcol='o_max_task', ycol='p_load', cat_col='i_elf', ax_curr=ax_curr)

    plt.savefig(fn_out + '.png')


def main(results_dir: str):
    print(f'running for data directory {results_dir}')
    df = _parse_results(results_dir)
    df = _postprocessing(df)
    _plot(results_dir, df)


if __name__ == '__main__':
    datas = [ll for ll in [x[0] for x in os.walk('.')] if ll.startswith('./data-')]
    do_show = True
    for data in datas:
        path_to_dir = os.path.join(os.path.dirname(__file__), data)
        main(path_to_dir)
    if do_show:
        plt.show()
